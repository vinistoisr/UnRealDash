#include "PackageInternal.h"
namespace dashboard_spec {
Error ImageHeader(std::string_view bytes, std::string_view path, ImageInfo &output) {
    const auto pointer = Join("/assets", path);
    if (bytes.size() < 33 || bytes.substr(0, 8) != std::string_view("\x89PNG\r\n\x1a\n", 8) ||
        bytes.substr(12, 4) != "IHDR")
        return Fail(ErrorCode::E_PKG_IMAGE_FORMAT_UNSUPPORTED, pointer, "expected PNG IHDR header");
    auto number = [&](std::size_t offset) {
        std::uint32_t n = 0;
        for (std::size_t i = 0; i < 4; ++i)
            n = (n << 8) | static_cast<unsigned char>(bytes[offset + i]);
        return n;
    };
    if (number(8) != 13 || static_cast<unsigned char>(bytes[24]) != 8 || bytes[26] != 0 || bytes[27] != 0 ||
        (bytes[28] != 0 && bytes[28] != 1))
        return Fail(ErrorCode::E_PKG_IMAGE_FORMAT_UNSUPPORTED, pointer, "unsupported PNG header");
    switch (static_cast<unsigned char>(bytes[25])) {
    case 6:
        output.format = "rgba8";
        break;
    case 2:
        output.format = "rgb8";
        break;
    case 0:
        output.format = "gray8";
        break;
    default:
        return Fail(ErrorCode::E_PKG_IMAGE_FORMAT_UNSUPPORTED, pointer, "unsupported PNG colour type");
    }
    output.width = number(16);
    output.height = number(20);
    if (output.width == 0 || output.height == 0 || output.width > limits::image_dimension ||
        output.height > limits::image_dimension)
        return Fail(ErrorCode::E_PKG_IMAGE_DIMENSIONS, pointer, "PNG pixel dimensions exceeded");
    return {};
}
} // namespace dashboard_spec
