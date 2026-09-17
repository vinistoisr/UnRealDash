#include "SignalCore/Recording.h"
#include "JsonLinesText.h"
#include <cmath>
#include <cstring>
namespace signal_core {
namespace {
constexpr const char *qualities[] = {"valid", "stale", "unavailable", "invalid"};
constexpr const char *ages[] = {"measured", "unknown"};
bool UnitText(json_lines::Reader &reader, Unit &unit) {
    char name[128]{};
    if (!reader.String(name))
        return false;
    auto parsed = ParseUnit(name);
    if (!parsed.Ok())
        return false;
    unit = *parsed.Get();
    return true;
}
} // namespace
Status WriteHeader(TextSink sink, std::span<const Signal> signals) {
    json_lines::Writer w(sink);
    w.Text("{\"format\":\"unrealdash-recording\",\"version\":1,\"signals\":[");
    bool first = true;
    for (const auto &s : signals) {
        if (!IsUnit(s.unit) || s.deadline <= Time::zero() || !std::memchr(s.name, 0, sizeof(s.name)))
            return Error(ErrorCode::invalid_configuration, "recording signal %u", s.id);
        if (!first)
            w.Text(",");
        first = false;
        w.Text("{\"id\":");
        w.Unsigned(s.id);
        w.Text(",\"name\":");
        w.String(s.name);
        w.Text(",\"unit\":");
        w.String(UnitName(s.unit));
        w.Text(",\"deadline_ns\":");
        w.Integer(s.deadline.count());
        w.Text(",\"discrete\":");
        w.Text(s.discrete ? "true" : "false");
        w.Text("}");
    }
    w.Text("]}\n");
    return w.Finish();
}
Status WriteSample(TextSink sink, const SignalSample &entry) {
    const auto &s = entry.sample;
    if (!IsUnit(s.unit) || static_cast<unsigned>(s.quality) > 3 || static_cast<unsigned>(s.age_evidence) > 1 ||
        (std::isinf(s.value) || (s.quality == Quality::valid && !std::isfinite(s.value))))
        return Error(ErrorCode::invalid_configuration, "recording sample %u", entry.signal);
    json_lines::Writer w(sink);
    w.Text("{\"t_recv_ns\":");
    w.Integer(s.t_recv.count());
    w.Text(",\"signal\":");
    w.Unsigned(entry.signal);
    w.Text(",\"value\":");
    w.Number(s.value);
    w.Text(",\"unit\":");
    w.String(UnitName(s.unit));
    w.Text(",\"source\":");
    w.Unsigned(s.source);
    w.Text(",\"quality\":");
    w.String(qualities[static_cast<unsigned>(s.quality)]);
    w.Text(",\"age_evidence\":");
    w.String(ages[static_cast<unsigned>(s.age_evidence)]);
    w.Text(",\"seq\":");
    w.Unsigned(s.seq);
    if (s.has_source_time) {
        w.Text(",\"t_source_ns\":");
        w.Integer(s.t_source.count());
    }
    w.Text(",\"generation\":");
    w.Unsigned(s.generation);
    w.Text("}\n");
    return w.Finish();
}
Result<RecordingView> ReadRecording(std::string_view text, std::span<Signal> signals, std::span<SignalSample> samples) {
    std::size_t offset = 0, line = 1, signal_count = 0, sample_count = 0;
    while (offset < text.size()) {
        const auto end = text.find('\n', offset);
        json_lines::Reader reader(
            text.substr(offset, end == std::string_view::npos ? text.size() - offset : end - offset), line, offset);
        if (end == std::string_view::npos)
            return reader.Failure();
        if (line == 1) {
            std::uint64_t version{};
            if (!reader.Token("{\"format\":\"unrealdash-recording\",\"version\":") || !reader.Unsigned(version))
                return reader.Failure();
            if (version != 1)
                return Error(ErrorCode::recording_version, "recording version %llu at line 1 byte offset %zu",
                             static_cast<unsigned long long>(version), offset);
            if (!reader.Token(",\"signals\":["))
                return reader.Failure();
            while (!reader.Peek(']')) {
                if (signal_count == signals.size())
                    return Error(ErrorCode::capacity_exceeded, "signals at line %zu byte offset %zu", line, offset);
                Signal s{};
                std::uint64_t id{};
                std::int64_t deadline{};
                if (signal_count && !reader.Token(","))
                    return reader.Failure();
                if (!reader.Token("{\"id\":") || !reader.Unsigned(id) || id > UINT32_MAX ||
                    !reader.Token(",\"name\":") || !reader.String(s.name) || !reader.Token(",\"unit\":") ||
                    !UnitText(reader, s.unit) || !reader.Token(",\"deadline_ns\":") || !reader.Integer(deadline) ||
                    deadline <= 0 || !reader.Token(",\"discrete\":") || !reader.Boolean(s.discrete) ||
                    !reader.Token("}"))
                    return reader.Failure();
                s.id = static_cast<SignalId>(id);
                s.deadline = Time(deadline);
                for (std::size_t i = 0; i < signal_count; ++i)
                    if (signals[i].id == s.id)
                        return reader.Failure();
                signals[signal_count++] = s;
            }
            if (!reader.Token("]}") || !reader.Done())
                return reader.Failure();
        } else {
            if (sample_count == samples.size())
                return Error(ErrorCode::capacity_exceeded, "samples at line %zu byte offset %zu", line, offset);
            SignalSample entry{};
            auto &s = entry.sample;
            std::int64_t received{}, source_time{};
            std::uint64_t id{};
            char quality[32]{}, age[32]{};
            if (!reader.Token("{\"t_recv_ns\":") || !reader.Integer(received) || !reader.Token(",\"signal\":") ||
                !reader.Unsigned(id) || id > UINT32_MAX || !reader.Token(",\"value\":") || !reader.Number(s.value) ||
                !reader.Token(",\"unit\":") || !UnitText(reader, s.unit) || !reader.Token(",\"source\":") ||
                !reader.Unsigned(s.source) || !reader.Token(",\"quality\":") || !reader.String(quality) ||
                !reader.Token(",\"age_evidence\":") || !reader.String(age) || !reader.Token(",\"seq\":") ||
                !reader.Unsigned(s.seq))
                return reader.Failure();
            if (reader.Token(",\"t_source_ns\":")) {
                if (!reader.Integer(source_time))
                    return reader.Failure();
                s.has_source_time = true;
                s.t_source = Time(source_time);
            }
            if (!reader.Token(",\"generation\":") || !reader.Unsigned(s.generation) || !reader.Token("}") ||
                !reader.Done())
                return reader.Failure();
            unsigned q = 0, a = 0;
            for (; q < 4 && std::string_view(quality) != qualities[q]; ++q) {
            }
            for (; a < 2 && std::string_view(age) != ages[a]; ++a) {
            }
            if (q == 4 || a == 2 || (q == 0 && !std::isfinite(s.value)))
                return reader.Failure();
            s.quality = static_cast<Quality>(q);
            s.age_evidence = static_cast<AgeEvidence>(a);
            s.t_recv = Time(received);
            entry.signal = static_cast<SignalId>(id);
            bool declared = false;
            for (std::size_t i = 0; i < signal_count; ++i)
                if (signals[i].id == entry.signal && QuantityOf(signals[i].unit) == QuantityOf(s.unit))
                    declared = true;
            if (!declared || (sample_count && (s.t_recv < samples[sample_count - 1].sample.t_recv ||
                                               s.generation < samples[sample_count - 1].sample.generation)))
                return reader.Failure();
            samples[sample_count++] = entry;
        }
        offset = end + 1;
        ++line;
    }
    if (line == 1)
        return Error(ErrorCode::recording_malformed_line, "recording line 1 byte offset 0");
    return RecordingView{signals.first(signal_count), samples.first(sample_count)};
}
Status Replay::Start(RecordingView recording, bool loop, Time loop_pause) {
    if (recording.samples.empty() || !clock_.now || (loop && loop_pause <= Time::zero()))
        return Error(ErrorCode::invalid_configuration, "replay samples or loop pause");
    Time previous = recording.samples.front().sample.t_recv;
    std::uint64_t previous_generation = recording.samples.front().sample.generation;
    for (const auto &entry : recording.samples) {
        if (entry.sample.t_recv < Time::zero() || entry.sample.t_recv < previous ||
            entry.sample.generation < previous_generation || entry.sample.generation == UINT64_MAX)
            return Error(ErrorCode::invalid_configuration, "replay timestamp or generation order for signal %u",
                         entry.signal);
        previous = entry.sample.t_recv;
        previous_generation = entry.sample.generation;
    }
    first_ = recording.samples.front().sample.t_recv;
    auto period = AddTime(recording.samples.back().sample.t_recv - first_, loop ? loop_pause : Time::zero());
    if (!period.Ok())
        return period.GetStatus();
    period_ = *period.Get();
    origin_ = clock_.Now();
    if (origin_ < Time::zero() || !AddTime(origin_, period_).Ok())
        return Error(ErrorCode::invalid_configuration, "replay clock range");
    generation_base_ = registry_.Generation();
    generation_span_ = recording.samples.back().sample.generation - recording.samples.front().sample.generation + 1;
    if (generation_base_ > UINT64_MAX - recording.samples.back().sample.generation)
        return Error(ErrorCode::invalid_configuration, "replay generation overflow");
    recording_ = recording;
    cursor_ = 0;
    loop_ = loop;
    return {};
}
Result<Time> Replay::NextTime() const {
    if (recording_.samples.empty() || Ended())
        return Error(ErrorCode::invalid_configuration, "replay has no next sample");
    if (cursor_ == recording_.samples.size())
        return origin_ + period_;
    return origin_ + (recording_.samples[cursor_].sample.t_recv - first_);
}
Result<bool> Replay::Step() {
    if (recording_.samples.empty())
        return Error(ErrorCode::invalid_configuration, "replay not started");
    return StepUntil(clock_.Now());
}
Result<bool> Replay::StepUntil(Time cutoff) {
    if (recording_.samples.empty())
        return Error(ErrorCode::invalid_configuration, "replay not started");
    if (cursor_ == recording_.samples.size()) {
        if (!loop_ || cutoff < origin_ + period_) {
            registry_.Expire();
            return false;
        }
        if (!AddTime(origin_ + period_, period_).Ok() || generation_base_ > UINT64_MAX - generation_span_ ||
            generation_base_ + generation_span_ > UINT64_MAX - recording_.samples.back().sample.generation)
            return Error(ErrorCode::invalid_configuration, "replay loop time or generation overflow");
        origin_ += period_;
        generation_base_ += generation_span_;
        cursor_ = 0;
        const auto status = registry_.SetGeneration(generation_base_ + recording_.samples.front().sample.generation);
        if (!status.Ok())
            return status;
    }
    const auto &entry = recording_.samples[cursor_];
    const auto due = origin_ + (entry.sample.t_recv - first_);
    if (cutoff < due) {
        registry_.Expire();
        return false;
    }
    Sample sample = entry.sample;
    sample.t_recv = due;
    sample.generation += generation_base_;
    const auto status = registry_.Apply(entry.signal, sample);
    if (!status.Ok())
        return status;
    ++cursor_;
    return true;
}
Status Replay::Poll() {
    if (recording_.samples.empty())
        return Error(ErrorCode::invalid_configuration, "replay not started");
    const auto cutoff = clock_.Now();
    for (;;) {
        const auto step = StepUntil(cutoff);
        if (!step.Ok())
            return step.GetStatus();
        if (!*step.Get())
            return {};
    }
}
} // namespace signal_core
