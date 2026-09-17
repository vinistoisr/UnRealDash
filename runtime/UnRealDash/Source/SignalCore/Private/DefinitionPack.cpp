#include "SignalCore/DefinitionPack.h"
#include <cmath>
namespace signal_core {
Status ValidatePack(const DefinitionPack &pack) {
    if (pack.frames.empty())
        return Error(ErrorCode::pack_empty_frame_list, "pack has no frames");
    for (std::size_t i = 0; i < pack.frames.size(); ++i) {
        const auto &frame = pack.frames[i];
        for (std::size_t j = 0; j < i; ++j)
            if (pack.frames[j].frame_id == frame.frame_id)
                return Error(ErrorCode::pack_duplicate_frame_id, "duplicate frame %u", frame.frame_id);
        if (i && pack.frames[i - 1].frame_id > frame.frame_id)
            return Error(ErrorCode::pack_frames_unsorted, "unsorted frame %u", frame.frame_id);
        for (const auto &field : frame.fields) {
            if (!field.width_bytes)
                return Error(ErrorCode::pack_zero_field_width, "frame %u field at %u has zero width", frame.frame_id,
                             field.byte_offset);
            if (static_cast<unsigned>(field.byte_offset) + field.width_bytes > frame.payload_length)
                return Error(ErrorCode::pack_field_past_payload_length, "frame %u field at %u exceeds payload",
                             frame.frame_id, field.byte_offset);
            if (!std::isfinite(field.scale) || field.scale == 0 || !std::isfinite(field.offset))
                return Error(ErrorCode::pack_invalid_scale, "frame %u field at %u has invalid affine conversion",
                             frame.frame_id, field.byte_offset);
            if (field.width_bytes > 4 || !IsUnit(field.unit))
                return Error(ErrorCode::invalid_configuration, "frame %u field at %u has unsupported width or unit",
                             frame.frame_id, field.byte_offset);
        }
    }
    return {};
}
} // namespace signal_core
