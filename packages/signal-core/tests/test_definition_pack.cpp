#include "support/TestPackData.h"
#include <doctest.h>
#include <limits>
using namespace signal_core;
namespace {
struct PackCase {
    FieldDefinition field{"field", 0, 0, 2};
    std::array<FrameDefinition, 2> frames{{{1, FrameRole::telemetry, 8, {&field, 1}}, {2, FrameRole::status, 8, {}}}};
    DefinitionPack pack{"test", "1", 1, frames};
};
} // namespace
TEST_CASE("2.9 field past the payload length is rejected") {
    PackCase p;
    p.field.byte_offset = 7;
    CHECK(ValidatePack(p.pack).code == ErrorCode::pack_field_past_payload_length);
}
TEST_CASE("2.9 duplicate frame id is rejected") {
    PackCase p;
    p.frames[1].frame_id = 1;
    CHECK(ValidatePack(p.pack).code == ErrorCode::pack_duplicate_frame_id);
}
TEST_CASE("2.9 unsorted frame list is rejected") {
    PackCase p;
    p.frames[1].frame_id = 0;
    CHECK(ValidatePack(p.pack).code == ErrorCode::pack_frames_unsorted);
}
TEST_CASE("2.9 zero field width is rejected") {
    PackCase p;
    p.field.width_bytes = 0;
    CHECK(ValidatePack(p.pack).code == ErrorCode::pack_zero_field_width);
}
TEST_CASE("2.9 empty frame list is rejected") {
    PackCase p;
    p.pack.frames = {};
    CHECK(ValidatePack(p.pack).code == ErrorCode::pack_empty_frame_list);
}
TEST_CASE("2.9 zero or non-finite scale is rejected") {
    for (double value : {0.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        PackCase p;
        p.field.scale = value;
        CHECK(ValidatePack(p.pack).code == ErrorCode::pack_invalid_scale);
    }
}
TEST_CASE("2.9 non-finite offset is rejected") {
    for (double value : {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        PackCase p;
        p.field.offset = value;
        CHECK(ValidatePack(p.pack).code == ErrorCode::pack_invalid_scale);
    }
}
TEST_CASE("2.9 a frame with an empty field list is accepted") {
    PackCase p;
    CHECK(ValidatePack(p.pack).Ok());
    CHECK(ValidatePack(telemetry_test::pack).Ok());
}
