#pragma once
#include "SignalCore/Export.h"
#include "SignalCore/SignalRegistry.h"
#include <string_view>
namespace signal_core {
// Injected output keeps file ownership, paths, and filesystem policy outside the library.
struct TextSink {
    void *context{};
    Status (*write)(void *, std::string_view){};
};
SIGNALCORE_API Status WriteHeader(TextSink sink, std::span<const Signal> signals);
SIGNALCORE_API Status WriteSample(TextSink sink, const SignalSample &sample);
struct RecordingView {
    std::span<const Signal> signals{};
    std::span<const SignalSample> samples{};
};
SIGNALCORE_API Result<RecordingView> ReadRecording(std::string_view text, std::span<Signal> signals,
                                                   std::span<SignalSample> samples);
// Loop pause is explicit, positive, and preserves a finite duration even for a one-sample recording.
// Poll at NextTime() and the shared expiry schedule's earliest time. EOF never disconnects.
class SIGNALCORE_API Replay {
  public:
    Replay(Clock clock, SignalRegistry &registry) : clock_(clock), registry_(registry) {}
    Status Start(RecordingView recording, bool loop, Time loop_pause);
    // Applies at most one due sample. True means the caller can evaluate rules before stepping again.
    // False means no sample is due; freshness is still expired against the injected clock.
    Result<bool> Step();
    // Drains samples due at the entry cutoff, for callers without per-sample rule evaluation.
    Status Poll();
    Result<Time> NextTime() const;
    bool Ended() const { return cursor_ == recording_.samples.size() && !loop_; }

  private:
    Result<bool> StepUntil(Time cutoff);
    Clock clock_;
    SignalRegistry &registry_;
    RecordingView recording_{};
    std::size_t cursor_{};
    Time origin_{};
    Time first_{};
    Time period_{};
    std::uint64_t generation_base_{};
    std::uint64_t generation_span_{};
    bool loop_{};
};
} // namespace signal_core
