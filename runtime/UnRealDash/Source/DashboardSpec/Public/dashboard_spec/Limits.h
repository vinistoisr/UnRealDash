#pragma once
#include "dashboard_spec/Export.h"
#include <cstddef>
namespace dashboard_spec::limits {
inline constexpr std::size_t expanded_size = 67108864;
inline constexpr std::size_t entry_count = 4096;
inline constexpr std::size_t asset_size = 16777216;
inline constexpr std::size_t document_size = 4194304;
inline constexpr std::size_t json_depth = 64;
inline constexpr std::size_t json_nodes = 200000;
inline constexpr std::size_t string_length = 65536;
inline constexpr std::size_t expression_depth = 32;
inline constexpr std::size_t expression_nodes = 20000;
inline constexpr std::size_t rule_nodes = 512;
inline constexpr std::size_t components = 2000;
inline constexpr std::size_t history_samples = 4096;
inline constexpr std::size_t image_dimension = 4096;
inline constexpr std::size_t mobile_texture = 201326592;
inline constexpr std::size_t desktop_texture = 536870912;
} // namespace dashboard_spec::limits
