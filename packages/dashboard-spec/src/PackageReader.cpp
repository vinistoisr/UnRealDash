#include "dashboard_spec/PackageReader.h"
#include "PackageInternal.h"
#include <limits>
#include <miniz.h>
namespace dashboard_spec {
namespace {
struct Entry {
    std::string name;
    std::filesystem::path file;
    std::uint64_t bytes{};
    mz_uint index{};
    bool directory{};
};
struct Archive {
    mz_zip_archive zip{};
    bool opened{};
    ~Archive() {
        if (opened)
            mz_zip_reader_end(&zip);
    }
};
bool JsonPath(const std::string &name) { return name.ends_with(".json"); }
Error Io(std::string_view path) {
    return Fail(ErrorCode::E_PKG_ZIP_MALFORMED, "", std::string("cannot read package entry ") + std::string(path));
}
std::uint64_t Little(std::string_view bytes, std::size_t offset, std::size_t width) {
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < width; ++i)
        value |= static_cast<std::uint64_t>(static_cast<unsigned char>(bytes[offset + i])) << (8 * i);
    return value;
}
Error ArchiveGate(const std::filesystem::path &path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream || stream.tellg() < 22)
        return Io(path.generic_string());
    const auto size = static_cast<std::uint64_t>(stream.tellg());
    // ZIP EOCD has a fixed 22-byte prefix and at most 65535 comment bytes.
    const auto tail_size = static_cast<std::size_t>(std::min<std::uint64_t>(size, 22 + 65535));
    std::string tail(tail_size, '\0');
    stream.seekg(static_cast<std::streamoff>(size - tail_size));
    stream.read(tail.data(), static_cast<std::streamsize>(tail.size()));
    if (!stream)
        return Io(path.generic_string());
    std::size_t eocd = tail.size();
    for (std::size_t i = tail.size() - 22;; --i) {
        if (Little(tail, i, 4) == 0x06054b50) {
            if (i + 22 + Little(tail, i + 20, 2) != tail.size())
                return Io(path.generic_string());
            eocd = i;
            break;
        }
        if (i == 0)
            break;
    }
    if (eocd == tail.size())
        return Io(path.generic_string());
    auto count = Little(tail, eocd + 10, 2);
    auto disk_count = Little(tail, eocd + 8, 2);
    auto central_size = Little(tail, eocd + 12, 4);
    auto central_offset = Little(tail, eocd + 16, 4);
    auto central_end = size - tail_size + eocd;
    if (Little(tail, eocd + 4, 2) || Little(tail, eocd + 6, 2))
        return Io(path.generic_string());
    const bool requires_zip64 = count == 0xffff || disk_count == 0xffff || central_size == 0xffffffff ||
                                central_offset == 0xffffffff;
    // miniz honours this locator even when every legacy field is small.
    std::string locator(20, '\0');
    if (central_end >= locator.size()) {
        stream.seekg(static_cast<std::streamoff>(central_end - 20));
        stream.read(locator.data(), 20);
        if (!stream)
            return Io(path.generic_string());
    }
    const bool has_zip64 = Little(locator, 0, 4) == 0x07064b50;
    if (requires_zip64 && !has_zip64)
        return Io(path.generic_string());
    if (has_zip64) {
        if (Little(locator, 4, 4) != 0 || Little(locator, 16, 4) != 1)
            return Io(path.generic_string());
        const auto offset = Little(locator, 8, 8);
        if (offset > central_end || central_end - offset < 56 + 20)
            return Io(path.generic_string());
        std::string record(56, '\0');
        stream.seekg(static_cast<std::streamoff>(offset));
        stream.read(record.data(), 56);
        if (!stream || Little(record, 0, 4) != 0x06064b50 || Little(record, 4, 8) < 44 ||
            Little(record, 4, 8) > central_end - offset - 32 || Little(record, 16, 4) || Little(record, 20, 4))
            return Io(path.generic_string());
        if (count != disk_count || Little(record, 24, 8) != Little(record, 32, 8))
            return Io(path.generic_string());
        disk_count = std::max(disk_count, Little(record, 24, 8));
        count = std::max(count, Little(record, 32, 8));
        central_size = std::max(central_size, Little(record, 40, 8));
        central_offset = Little(record, 48, 8);
        central_end = offset;
    }
    // Check both entry declarations before miniz can allocate a central directory.
    if (count > limits::entry_count || disk_count > limits::entry_count)
        return Fail(ErrorCode::E_PKG_ENTRY_COUNT, "/entries", "declared package entry count exceeded");
    constexpr auto central_bound =
        std::min(limits::expanded_size, limits::entry_count * (46 + 3 * limits::string_length));
    if (central_size > central_bound)
        return Fail(ErrorCode::E_PKG_EXPANDED_SIZE, "/entries", "central directory byte bound exceeded");
    if (count != disk_count || central_offset > central_end || central_size > central_end - central_offset)
        return Io(path.generic_string());
    // The record that passed the checks above must describe a real central directory: a header signature at
    // the declared offset. A forged record planted in the archive comment cannot fake the bytes it points at
    // without also being the record miniz will use, so the gate and miniz stay in agreement.
    if (count > 0) {
        std::string header(4, '\0');
        stream.seekg(static_cast<std::streamoff>(central_offset));
        stream.read(header.data(), 4);
        if (!stream || Little(header, 0, 4) != 0x02014b50)
            return Io(path.generic_string());
    }
    return {};
}
} // namespace
Error PackageReader::Read(std::string_view path, Profile profile) const {
    std::error_code ec;
    const std::filesystem::path root(path);
    const auto status = std::filesystem::symlink_status(root, ec);
    if (ec)
        return Io(path);
    if (std::filesystem::is_symlink(status))
        return Fail(ErrorCode::E_PKG_PATH_LINK, "", path);
    const bool directory = std::filesystem::is_directory(status);
    Archive archive;
    PathResolver resolver;
    std::map<std::string, Entry> entries;
    std::uint64_t total = 0;
    std::size_t count = 0;
    auto add = [&](std::string name, std::filesystem::path file, std::uint64_t size, mz_uint index, bool is_directory,
                   bool link) -> Error {
        if (++count > limits::entry_count)
            return Fail(ErrorCode::E_PKG_ENTRY_COUNT, "/entries", "package entry count exceeded");
        std::string normalized;
        auto error = resolver.Add(name, link, normalized);
        if (!error.Ok())
            return error;
        if (size > limits::expanded_size || total > limits::expanded_size - size)
            return Fail(ErrorCode::E_PKG_EXPANDED_SIZE, "/entries", "package expanded size exceeded");
        total += size;
        entries.emplace(normalized, Entry{normalized, std::move(file), size, index, is_directory});
        return {};
    };
    if (directory) {
        const auto canonical_root = std::filesystem::canonical(root, ec);
        if (ec)
            return Io(path);
        std::filesystem::recursive_directory_iterator it(root, std::filesystem::directory_options::none, ec), end;
        if (ec)
            return Io(path);
        for (; it != end; it.increment(ec)) {
            if (ec)
                return Io(path);
            const auto file = it->path();
            const auto relative = file.lexically_relative(root).generic_string();
            const auto st = it->symlink_status(ec);
            if (ec)
                return Io(relative);
            bool link = std::filesystem::is_symlink(st);
            const bool is_directory = std::filesystem::is_directory(st);
            if (!link && !is_directory) {
                const auto links = std::filesystem::hard_link_count(file, ec);
                if (ec)
                    return Io(relative);
                link = links > 1;
            }
            if (!link) {
                const auto resolved = std::filesystem::canonical(file, ec);
                if (ec)
                    return Io(relative);
                const auto rel = resolved.lexically_relative(canonical_root);
                if (rel.empty() || rel.generic_string().starts_with(".."))
                    link = true;
            }
            const auto size = (!link && !is_directory) ? std::filesystem::file_size(file, ec) : 0;
            if (ec)
                return Io(relative);
            auto error = add(relative, file, size, 0, is_directory, link);
            if (!error.Ok())
                return error;
        }
        if (ec)
            return Io(path);
    } else {
        const auto gate = ArchiveGate(root);
        if (!gate.Ok())
            return gate;
        archive.opened = mz_zip_reader_init_file(&archive.zip, root.string().c_str(), 0) != 0;
        if (!archive.opened)
            return Io(path);
        const auto files = mz_zip_reader_get_num_files(&archive.zip);
        if (files > limits::entry_count)
            return Fail(ErrorCode::E_PKG_ENTRY_COUNT, "/entries", "package entry count exceeded");
        for (mz_uint i = 0; i < files; ++i) {
            mz_zip_archive_file_stat stat{};
            if (!mz_zip_reader_file_stat(&archive.zip, i, &stat))
                return Io(path);
            const auto length = mz_zip_reader_get_filename(&archive.zip, i, nullptr, 0);
            if (length == 0 || length > limits::string_length)
                return Io(path);
            std::string name(length, '\0');
            if (!mz_zip_reader_get_filename(&archive.zip, i, name.data(), length))
                return Io(path);
            name.resize(length - 1);
            const auto mode = (stat.m_external_attr >> 16) & 0170000;
            const bool dos_reparse = (stat.m_external_attr & 0x400) != 0;
            const bool dos_directory = (stat.m_external_attr & 0x10) != 0;
            // Windows tools write no Unix type bits at all (mode 0) and only DOS attributes; such an entry is
            // a regular file unless the DOS bits say directory or reparse point.
            const bool regular =
                (mode == 0100000 || (mode == 0 && !dos_directory && !dos_reparse)) && !stat.m_is_directory && !dos_directory;
            const bool folder = (mode == 0040000 || (mode == 0 && dos_directory)) && stat.m_is_directory;
            const bool link = dos_reparse || (!regular && !folder);
            auto error = add(name, {}, stat.m_uncomp_size, i, stat.m_is_directory != 0, link);
            if (!error.Ok())
                return error;
        }
    }
    for (const auto &[name, entry] : entries) {
        if (entry.directory)
            continue;
        if (JsonPath(name) && entry.bytes > limits::document_size)
            return Fail(ErrorCode::E_DOC_TOO_LARGE, Join("/entries", name), name);
        if (!JsonPath(name) && entry.bytes > limits::asset_size)
            return Fail(ErrorCode::E_PKG_ASSET_SIZE, Join("/entries", name), name);
    }
    auto read = [&](const Entry &entry, std::size_t limit, bool complete, std::string &bytes) -> Error {
        bytes.clear();
        if (directory) {
            if (!ReadBounded(entry.file, limit, bytes))
                return Io(entry.name);
            if (!complete && bytes.size() > limit)
                bytes.resize(limit);
            if (complete && bytes.size() > limit)
                return Fail(ErrorCode::E_DOC_TOO_LARGE, Join("/entries", entry.name), entry.name);
            if (complete && bytes.size() != entry.bytes)
                return Io(entry.name);
            return {};
        }
        auto *iterator = mz_zip_reader_extract_iter_new(&archive.zip, entry.index, 0);
        if (!iterator)
            return Io(entry.name);
        std::array<char, 8192> block{};
        const auto cap = limit + (complete ? 1 : 0);
        bool failed = false;
        while (bytes.size() < cap) {
            const auto amount = std::min(block.size(), cap - bytes.size());
            const auto got = mz_zip_reader_extract_iter_read(iterator, block.data(), amount);
            bytes.append(block.data(), got);
            if (iterator->status < 0) {
                failed = true;
                break;
            }
            if (!got)
                break;
        }
        const bool finished = iterator->status == TINFL_STATUS_DONE && iterator->out_blk_remain == 0;
        const bool valid = mz_zip_reader_extract_iter_free(iterator) != 0;
        if (complete && bytes.size() > limit)
            return Fail(ErrorCode::E_DOC_TOO_LARGE, Join("/entries", entry.name), entry.name);
        if (failed || (complete && (!finished || !valid || bytes.size() != entry.bytes)))
            return Io(entry.name);
        return {};
    };
    Document bundle;
    bundle.Data().json.SetObject();
    auto &allocator = bundle.Data().json.GetAllocator();
    for (const auto &[name, entry] : entries)
        if (JsonPath(name) && !entry.directory) {
            const auto stem = std::filesystem::path(name).stem().string();
            if (name != stem + ".json" || (stem != "dashboard" && stem != "manifest" && stem != "signals" &&
                                           stem != "showcase" && stem != "definition-pack"))
                return Fail(ErrorCode::E_PKG_UNSUPPORTED_DOCUMENT, Join("/entries", name), name);
            std::string bytes;
            auto error = read(entry, limits::document_size, true, bytes);
            if (!error.Ok())
                return error;
            Document doc;
            error = BoundedParse(bytes, doc);
            if (!error.Ok())
                return error;
            Value key(stem.c_str(), allocator);
            Value value;
            value.CopyFrom(doc.Data().json, allocator);
            bundle.Data().json.AddMember(key, value, allocator);
        }
    auto error = validator_.Validate(bundle);
    if (!error.Ok())
        return error;
    const auto &b = bundle.Data().json;
    if (!b.HasMember("manifest") || !b.HasMember("dashboard"))
        return Fail(ErrorCode::E_SCHEMA, "", "package needs manifest.json and dashboard.json");
    PathResolver metadata_resolver;
    std::map<std::string, const Value *> metadata;
    std::map<std::string, std::string> metadata_keys;
    const auto &assets = b["manifest"]["assets"];
    for (const auto &key : Keys(assets)) {
        std::string normalized;
        error = metadata_resolver.Add(key, false, normalized);
        if (!error.Ok())
            return error;
        metadata.emplace(normalized, &At(assets, key));
        metadata_keys.emplace(normalized, key);
    }
    std::set<std::string> referenced;
    for (const auto &key : Keys(b["dashboard"]["components"])) {
        const auto &c = At(b["dashboard"]["components"], key);
        if (Text(c["type"]) == "image") {
            std::string normalized;
            error = resolver.Resolve(Text(c["properties"]["asset"]), normalized);
            if (!error.Ok())
                return error;
            referenced.insert(normalized);
        }
    }
    std::uint64_t texture = 0;
    const auto budget = profile == Profile::mobile ? limits::mobile_texture : limits::desktop_texture;
    for (const auto &name : referenced) {
        const auto found = metadata.find(name);
        if (found == metadata.end())
            return Fail(ErrorCode::E_IMAGE_NOT_IN_MANIFEST, Join("/manifest/assets", name), name);
        const auto &declared = *found->second;
        const auto pointer = Join("/manifest/assets", metadata_keys.at(name));
        const auto &entry = entries.at(name);
        std::string header;
        error = read(entry, 33, false, header);
        if (!error.Ok())
            return error;
        ImageInfo actual;
        error = ImageHeader(header, name, actual);
        if (!error.Ok())
            return error;
        if (declared["width"].GetDouble() != actual.width || declared["height"].GetDouble() != actual.height ||
            Text(declared["format"]) != actual.format ||
            declared["bytes"].GetDouble() != static_cast<double>(entry.bytes)) {
            const auto describe = [](double w, double h, const std::string &f, double bytes) {
                return std::to_string(w) + "x" + std::to_string(h) + " " + f + " bytes=" + std::to_string(bytes);
            };
            return Fail(ErrorCode::E_PKG_IMAGE_HEADER_MISMATCH, pointer,
                        "declared " +
                            describe(declared["width"].GetDouble(), declared["height"].GetDouble(),
                                     Text(declared["format"]), declared["bytes"].GetDouble()) +
                            "; read " +
                            describe(actual.width, actual.height, actual.format, static_cast<double>(entry.bytes)));
        }
        const std::uint64_t channels = actual.format == "rgba8" ? 4 : actual.format == "rgb8" ? 3 : 1;
        texture += static_cast<std::uint64_t>(actual.width) * actual.height * channels;
        if (texture > budget)
            return Fail(ErrorCode::E_PKG_TEXTURE_BUDGET, pointer, name);
    }
    return {};
}
} // namespace dashboard_spec
