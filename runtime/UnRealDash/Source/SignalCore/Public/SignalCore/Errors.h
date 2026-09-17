#pragma once
#include "SignalCore/Export.h"
#include <cstdint>
namespace signal_core {
enum class ErrorCode : std::uint16_t {
    none = 0,
    unknown_unit = 1,
    incompatible_units = 2,
    invalid_configuration = 3,
    capacity_exceeded = 4,
    missing_signal = 5,
    old_generation = 6,
    unknown_operation = 7,
    expression_depth = 8,
    expression_nodes = 9,
    malformed_expression = 10,
    missing_literal_unit = 11,
    recording_malformed_line = 12,
    recording_version = 13,
    io_error = 14,
    non_finite = 15,
    invalid_sequence = 16,
    pack_duplicate_frame_id,
    pack_frames_unsorted,
    pack_field_past_payload_length,
    pack_zero_field_width,
    pack_empty_frame_list,
    pack_invalid_scale,
    need_more_data
};
struct Status {
    ErrorCode code{};
    char message[256]{};
    bool Ok() const { return code == ErrorCode::none; }
};
SIGNALCORE_API Status Error(ErrorCode code, const char *format, ...);
template <class T> class Result {
  public:
    Result(T value) : value_(value) {}
    Result(Status status) : status_(status) {}
    bool Ok() const { return status_.Ok(); }
    const Status &GetStatus() const { return status_; }
    const T *Get() const { return Ok() ? &value_ : nullptr; }
    T *Get() { return Ok() ? &value_ : nullptr; }

  private:
    T value_{};
    Status status_{};
};
} // namespace signal_core
