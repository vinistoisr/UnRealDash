#include "SignalCore/Acquisition.h"
#include <algorithm>
#include <cstring>

namespace signal_core {
AcquisitionPipeline::AcquisitionPipeline(Clock clock, SignalRegistry &registry, ExpirySchedule &schedule,
                                         std::span<RuleEngine> rules, std::span<RuleResult> previous_results,
                                         std::span<std::uint8_t> read_buffer, SampleQueue &display,
                                         ITransport &transport, ISession &session, IDecoder &decoder, IMapping &mapping,
                                         IAcquisitionEventSink &sink)
    : clock_(clock), registry_(registry), schedule_(schedule), rules_(rules), previous_(previous_results),
      read_buffer_(read_buffer), latched_(read_buffer.first(std::min(read_buffer.size(), rules.size()))),
      display_(display), transport_(transport), session_(session), decoder_(decoder), mapping_(mapping), sink_(sink) {
    if (!clock.now || read_buffer.empty() || previous_results.size() != rules.size() ||
        read_buffer.size() < rules.size())
        configuration_ = Error(ErrorCode::invalid_configuration, "acquisition clock or caller storage mismatch");
    else
        for (std::size_t i = 0; i < rules.size(); ++i)
            previous_[i] = rules[i].Current();
    health_.generation = registry.Generation();
}
Status AcquisitionPipeline::Remember(Status status) {
    if (!status.Ok()) {
        std::memcpy(health_.last_error, status.message, sizeof(health_.last_error) - 1);
        health_.last_error[sizeof(health_.last_error) - 1] = 0;
    }
    return status;
}
Status AcquisitionPipeline::Start() {
    if (!configuration_.Ok())
        return Remember(configuration_);
    if (health_.connected)
        return {};
    session_.Reset();
    decoder_.Reset();
    return Connect();
}
Status AcquisitionPipeline::Connect() {
    if (health_.generation == UINT64_MAX)
        return Remember(Error(ErrorCode::invalid_configuration, "generation overflow"));
    auto status = transport_.Connect();
    if (!status.Ok())
        return Remember(status);
    health_.connected = transport_.Connected();
    if (!health_.connected)
        return Remember(Error(ErrorCode::io_error, "connect succeeded without connection"));
    ++health_.generation;
    status = registry_.SetGeneration(health_.generation);
    for (auto &rule : rules_)
        rule.ClearLatch();
    return Remember(status);
}
Status AcquisitionPipeline::Stop() {
    if (!health_.connected && !transport_.Connected())
        return {};
    auto status = transport_.Disconnect();
    health_.connected = transport_.Connected();
    return Remember(status);
}
Status AcquisitionPipeline::Reconnect() {
    if (!configuration_.Ok())
        return Remember(configuration_);
    auto status = transport_.Disconnect();
    health_.connected = transport_.Connected();
    session_.Reset();
    decoder_.Reset();
    if (!status.Ok())
        return Remember(status);
    status = Connect();
    if (status.Ok())
        ++health_.reconnects;
    return status;
}
Status AcquisitionPipeline::Acknowledge(std::uint32_t rule_id) {
    if (rule_id >= 64)
        return Error(ErrorCode::invalid_configuration, "acknowledge rule %u exceeds 63", rule_id);
    acknowledges_.fetch_or(std::uint64_t{1} << rule_id, std::memory_order_release);
    return {};
}
void AcquisitionPipeline::Transition(std::size_t index, Time now, PumpResult &result) {
    const auto next = rules_[index].Current();
    if (next.current != previous_[index].current || next.latched != previous_[index].latched) {
        ++transitions_;
        ++result.transitions;
        sink_.OnRuleTransition(rules_[index].RuleId(), now, next.current, next.latched);
    }
    previous_[index] = next;
}
void AcquisitionPipeline::Evaluate(Time now, PumpResult &result) {
    for (std::size_t i = 0; i < rules_.size(); ++i) {
        rules_[i].Evaluate(registry_.Samples(), health_.generation);
        Transition(i, now, result);
    }
    ++evaluations_;
    ++result.rule_evaluations;
}
PumpResult AcquisitionPipeline::Pump(Time deadline) {
    PumpResult result{};
    if (!configuration_.Ok()) {
        result.status = Remember(configuration_);
        return result;
    }
    const Time now = clock_.Now();
    bool failed = false;
    const auto Fail = [&](Status status) {
        result.status = Remember(status);
        failed = true;
    };
    while (health_.connected && !failed) {
        if (clock_.Now() > deadline)
            break;
        std::size_t written{};
        auto status = transport_.Read(read_buffer_, written);
        if (written > read_buffer_.size()) {
            Fail(Error(ErrorCode::capacity_exceeded, "transport read exceeds buffer"));
            break;
        }
        if (written) {
            health_.last_byte_at = now;
            health_.bytes += written;
            result.bytes_read += static_cast<std::uint32_t>(written);
        }
        if (status.code == ErrorCode::need_more_data && written == 0)
            break;
        if (!status.Ok()) {
            Fail(status);
            break;
        }
        if (!written)
            break;
        std::size_t offset{};
        while (offset < written && !failed) {
            std::size_t consumed{};
            status = session_.Offer(read_buffer_.subspan(offset, written - offset), consumed);
            if (!status.Ok()) {
                Fail(status);
                break;
            }
            if (!consumed || consumed > written - offset) {
                Fail(Error(ErrorCode::invalid_configuration, "session Offer made invalid progress"));
                break;
            }
            offset += consumed;
            Message message{};
            while (!failed && session_.Next(message) == Pending::produced) {
                status = decoder_.Offer(message);
                if (!status.Ok()) {
                    Fail(status);
                    break;
                }
                DecodedField decoded{}, mapped{};
                while (!failed && decoder_.Next(decoded) == Pending::produced) {
                    const auto decision = mapping_.Map(decoded, mapped);
                    if (decision == Pending::done) {
                        ++mapping_drops_;
                        ++result.mapping_drops;
                        continue;
                    }
                    if (decision != Pending::produced) {
                        Fail(Error(ErrorCode::invalid_configuration, "mapping needs data for a complete field"));
                        break;
                    }
                    if (mapped.sample.generation > health_.generation) {
                        Fail(Error(ErrorCode::invalid_configuration, "sample claims an unconnected generation"));
                        break;
                    }
                    status = registry_.Apply(mapped.signal, mapped.sample);
                    if (status.code == ErrorCode::old_generation)
                        continue;
                    if (!status.Ok()) {
                        Fail(status);
                        break;
                    }
                    ++applied_;
                    ++result.samples_applied;
                    registry_.Expire();
                    // This pass must precede display admission, for EVERY applied sample.
                    Evaluate(now, result);
                    if (display_.Push({mapped.signal, mapped.sample, true}))
                        ++result.display_pushed;
                    else {
                        ++display_drops_;
                        ++result.display_dropped;
                    }
                }
            }
        }
    }
    const auto bits = acknowledges_.exchange(0, std::memory_order_acq_rel);
    for (std::size_t i = 0; i < rules_.size(); ++i) {
        const auto id = rules_[i].RuleId();
        if (id < 64 && (bits & (std::uint64_t{1} << id))) {
            rules_[i].Acknowledge(id);
            Transition(i, now, result);
        }
    }
    Expiry expiry{};
    while (schedule_.PopDue(now, expiry)) {
        ++result.expiries_fired;
        registry_.Expire();
        Evaluate(now, result);
    }
    for (std::size_t i = 0; i < rules_.size(); ++i)
        latched_[i] = rules_[i].Current().latched ? 1 : 0;
    health_.connected = transport_.Connected();
    const auto status = registry_.Publish(latched_);
    if (status.Ok())
        ++published_;
    else if (!failed)
        Fail(status);
    return result;
}
} // namespace signal_core
