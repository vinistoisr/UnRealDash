#pragma once
#include "SignalCore/Units.h"
#include <span>
namespace signal_core {
enum class Acquisition : std::uint8_t { live, held };
struct FieldDefinition {
    const char *name{};
    std::uint32_t signal{};
    std::uint8_t byte_offset{};
    std::uint8_t width_bytes{};
    bool is_signed{};
    bool little_endian{true};
    double scale{1.0};
    double offset{};
    Unit unit{};
    Acquisition acquisition{};
    std::span<const std::int64_t> sentinels{};
};
enum class FrameRole : std::uint8_t { telemetry, status };
struct FrameDefinition {
    std::uint32_t frame_id{};
    FrameRole role{};
    std::uint8_t payload_length{};
    std::span<const FieldDefinition> fields{};
};
struct DefinitionPack {
    const char *id{};
    const char *version{};
    std::uint32_t api{};
    std::span<const FrameDefinition> frames{};
};
SIGNALCORE_API Status ValidatePack(const DefinitionPack &pack);
} // namespace signal_core
