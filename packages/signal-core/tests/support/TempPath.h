#pragma once
#include <filesystem>
#include <string>
struct TempPath {
    std::filesystem::path path;
    explicit TempPath(const char *name) {
        std::error_code error;
        path = std::filesystem::temp_directory_path(error) / name;
    }
    ~TempPath() {
        std::error_code error;
        std::filesystem::remove(path, error);
    }
};
