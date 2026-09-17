#include "../tests/support/TestPackData.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <new>
#include <random>
#include <sstream>
#include <string>
#include <vector>
#ifdef _MSC_VER
#include <malloc.h>
#endif
namespace {
// Executable-only allocation instrumentation, inactive while constructing inputs.
struct AllocationProbe {
    bool active{};
    std::uint64_t count{};
};
AllocationProbe &Probe() {
    static thread_local AllocationProbe probe;
    return probe;
}
void CountAllocation() {
    if (Probe().active)
        ++Probe().count;
}
} // namespace
void *operator new(std::size_t size) {
    CountAllocation();
    if (auto *p = std::malloc(size ? size : 1))
        return p;
    std::abort();
}
void *operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }
void operator delete[](void *p, std::size_t) noexcept { std::free(p); }
void *operator new(std::size_t size, std::align_val_t alignment) {
    CountAllocation();
    const auto align = static_cast<std::size_t>(alignment);
#ifdef _MSC_VER
    void *p = _aligned_malloc(size ? size : 1, align);
#else
    void *p = std::aligned_alloc(align, ((size ? size : 1) + align - 1) / align * align);
#endif
    if (!p)
        std::abort();
    return p;
}
void *operator new[](std::size_t size, std::align_val_t align) { return ::operator new(size, align); }
void operator delete(void *p, std::align_val_t) noexcept {
#ifdef _MSC_VER
    _aligned_free(p);
#else
    std::free(p);
#endif
}
void operator delete[](void *p, std::align_val_t a) noexcept { ::operator delete(p, a); }
void operator delete(void *p, std::size_t, std::align_val_t a) noexcept { ::operator delete(p, a); }
void operator delete[](void *p, std::size_t, std::align_val_t a) noexcept { ::operator delete(p, a); }
namespace {
using namespace signal_core;
struct Input {
    std::vector<std::byte> bytes;
    std::vector<std::uint64_t> boundaries;
};
struct Context {
    BinaryTelemetryV1Decoder &decoder;
    const Input &input;
    std::uint64_t false_boundary_frames{};
};
void Sample(void *, const signal_core::Sample &) {}
void Frame(void *p, const FrameEvent &event) {
    auto &c = *static_cast<Context *>(p);
    if (std::find(c.input.boundaries.begin(), c.input.boundaries.end(), event.stream_offset) ==
        c.input.boundaries.end())
        ++c.false_boundary_frames;
    c.decoder.OnFrame(event, Sample, nullptr);
}
bool Number(const char *text, std::uint64_t &value) {
    if (!*text || *text == '-')
        return false;
    char *end{};
    value = std::strtoull(text, &end, 10);
    return end && !*end;
}
} // namespace
int main(int argc, char **argv) {
    std::uint64_t limit = 1000000, seconds = 30, seed = 1234;
    std::string directory;
    for (int i = 1; i < argc; i += 2) {
        if (i + 1 >= argc)
            return 2;
        const std::string option = argv[i];
        if (option == "--corpus")
            directory = argv[i + 1];
        else {
            std::uint64_t value{};
            if (!Number(argv[i + 1], value))
                return 2;
            if (option == "--inputs")
                limit = value;
            else if (option == "--seconds")
                seconds = value;
            else if (option == "--seed")
                seed = value;
            else
                return 2;
        }
    }
    std::vector<Input> corpus;
    std::error_code error;
    std::vector<std::filesystem::path> paths;
    std::filesystem::directory_iterator it(directory, error), end;
    if (error) {
        std::cerr << "Cannot read corpus\n";
        return 2;
    }
    for (; it != end; it.increment(error)) {
        if (error)
            return 2;
        if (it->path().extension() == ".hex")
            paths.push_back(it->path());
    }
    std::sort(paths.begin(), paths.end());
    for (const auto &path : paths) {
        Input input;
        std::ifstream file(path);
        std::string line;
        while (std::getline(file, line)) {
            if (line.starts_with("# boundaries:")) {
                std::istringstream values(line.substr(13));
                std::uint64_t value;
                while (values >> value)
                    input.boundaries.push_back(value);
            } else if (!line.starts_with('#')) {
                std::istringstream values(line);
                std::string token;
                while (values >> token) {
                    if (token.size() != 2 || token.find_first_not_of("0123456789abcdefABCDEF") != token.npos)
                        return 2;
                    input.bytes.push_back(static_cast<std::byte>(std::strtoul(token.c_str(), nullptr, 16)));
                }
            }
        }
        if (!file.eof())
            return 2;
        corpus.push_back(std::move(input));
    }
    if (corpus.size() < 8 || !limit)
        return 2;
    std::mt19937_64 random(seed);
    FramerCounters framing;
    DecoderCounters decoding;
    std::uint64_t count = 0, false_locks = 0, false_boundary_frames = 0, streams_with_lock = 0;
    constexpr std::uint64_t reconnects = 0;
    const auto started = std::chrono::steady_clock::now();
    while (count < limit) {
        if (seconds && std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count() >=
                           static_cast<double>(seconds))
            break;
        Input input;
        if (count < corpus.size())
            input = corpus[static_cast<std::size_t>(count)];
        else if (random() % 2 == 0) {
            input = corpus[static_cast<std::size_t>(random() % corpus.size())];
            if (!input.bytes.empty())
                input.bytes[static_cast<std::size_t>(random() % input.bytes.size())] ^=
                    static_cast<std::byte>(1u << (random() % 8));
        } else {
            input.bytes.resize(static_cast<std::size_t>(random() % 65));
            for (auto &byte : input.bytes)
                byte = static_cast<std::byte>(random() & 255);
            const auto records = random() % 9;
            for (std::uint64_t i = 0; i < records; ++i) {
                input.boundaries.push_back(input.bytes.size());
                auto record =
                    telemetry_test::Record(telemetry_test::identifiers[static_cast<std::size_t>(random() % 21)]);
                for (std::size_t j = 8; j < 16; ++j)
                    record[j] = static_cast<std::byte>(random() & 255);
                if (random() % 4 == 0) {
                    const auto tag = telemetry_test::Record();
                    std::copy_n(tag.begin(), 8, record.begin() + 8);
                }
                input.bytes.insert(input.bytes.end(), record.begin(), record.end());
            }
        }
        Probe().active = true;
        const Clock clock{nullptr, [](void *) { return Time{1}; }};
        BinaryTelemetryV1Framer framer(telemetry_test::identifiers);
        BinaryTelemetryV1Decoder decoder(telemetry_test::pack, clock);
        Context context{decoder, input};
        for (std::size_t pos = 0; pos < input.bytes.size();) {
            const auto n = std::min(input.bytes.size() - pos, static_cast<std::size_t>(1 + random() % 31));
            decoder.OnBytesReceived();
            framer.Feed(std::span(input.bytes).subspan(pos, n), Frame, &context);
            pos += n;
        }
        framer.EndOfStream();
        Probe().active = false;
        const auto &f = framer.Counters();
        const auto &d = decoder.Counters();
        framing.frames_emitted += f.frames_emitted;
        framing.resyncs += f.resyncs;
        framing.locks += f.locks;
        framing.unknown_identifier_dropped += f.unknown_identifier_dropped;
        framing.bytes_discarded += f.bytes_discarded;
        framing.unconfirmed_candidates_discarded += f.unconfirmed_candidates_discarded;
        decoding.samples_published += d.samples_published;
        decoding.unknown_identifier_dropped += d.unknown_identifier_dropped;
        decoding.status_records += d.status_records;
        decoding.sentinel_rejections += d.sentinel_rejections;
        decoding.non_finite_results += d.non_finite_results;
        if (f.locks > 0) {
            ++streams_with_lock;
            false_locks += f.locks - 1 - reconnects;
        }
        false_boundary_frames += context.false_boundary_frames;
        ++count;
    }
    std::cout << "inputs=" << count << "\nelapsed_seconds="
              << std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count() << "\nseed=" << seed
              << "\nframes_emitted=" << framing.frames_emitted << "\nresyncs=" << framing.resyncs
              << "\nlocks=" << framing.locks
              << "\nframer_unknown_identifier_dropped=" << framing.unknown_identifier_dropped
              << "\nbytes_discarded=" << framing.bytes_discarded
              << "\nunconfirmed_candidates_discarded=" << framing.unconfirmed_candidates_discarded
              << "\nsamples_published=" << decoding.samples_published
              << "\nunknown_identifier_dropped=" << decoding.unknown_identifier_dropped
              << "\nstatus_records=" << decoding.status_records
              << "\nsentinel_rejections=" << decoding.sentinel_rejections << "\nfalse_locks=" << false_locks
              << "\nnon_finite_results=" << decoding.non_finite_results << "\nstreams_with_lock=" << streams_with_lock
              << "\nreconnects=" << reconnects << "\nfalse_boundary_frames=" << false_boundary_frames
              << "\nallocations=" << Probe().count << '\n';
    return Probe().count ? 1 : 0;
}
