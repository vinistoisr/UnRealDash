#pragma once
#include "SignalCore/Clock.h"
#include "SignalCore/Errors.h"
#include "SignalCore/Export.h"
#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <span>

namespace signal_core {

// PLAN 4.8's raw event log: records appended incrementally and flushed at least once per second,
// so a crash or a kill leaves the evidence up to that second rather than an empty file.
//
// It owns the file and the flush cadence and nothing else. What a record MEANS belongs to whoever
// writes it, because the five record groups 4.8 declares are produced by four different parts of
// the system and a writer that knew all five would be the one place every one of them could break.
// That includes the schema and header records: EventLog does not know which record types exist,
// so the owner writes those two first and flushes them before anything else.
//
// ONE THREAD WRITES. EventLog has no lock, and a record is two stdio calls, so two threads calling
// Write would interleave a row and its newline and produce a line no reader can parse. The design
// that avoids needing a lock is EventRowRing below: every other thread formats its row and pushes
// it, and the single writer thread drains and writes. The frame path therefore does no locking, no
// allocation and no syscall, which a mutex on the frame path would not have given.

// Builds one JSON record into caller-owned storage, with no allocation and no formatting library.
//
// A row is written on the frame path, so it cannot allocate and cannot be allowed to half-write:
// overflow is sticky and Ok() stays false, so the caller drops a whole row rather than appending a
// broken one that would cost the reader the rest of the file.
class SIGNALCORE_API RowBuilder {
  public:
    explicit RowBuilder(std::span<char> into) : into_(into) {}

    // Opens the object and writes the type field, which every record carries so a reader can find
    // its declared column schema.
    RowBuilder &Begin(const char *type);
    RowBuilder &Key(const char *name);
    RowBuilder &Integer(std::int64_t value);
    RowBuilder &Unsigned(std::uint64_t value);
    // Not-a-number and the infinities are written as null: JSON has no spelling for them, and a
    // reader that hits a bare NaN token stops at that line.
    RowBuilder &Number(double value);
    RowBuilder &Boolean(bool value);
    RowBuilder &String(const char *value);
    RowBuilder &Null();
    // Opens a nested array under the current key. Elements are written with the scalar calls.
    RowBuilder &BeginArray();
    RowBuilder &EndArray();
    // Opens a nested object under the current key, or as an array element.
    RowBuilder &BeginObject();
    RowBuilder &EndObject();
    RowBuilder &End();

    bool Ok() const { return ok_; }
    std::size_t Size() const { return size_; }
    std::span<const char> View() const { return {into_.data(), size_}; }
    void Reset();

  private:
    void Raw(const char *text);
    // The quoted, escaped form with no separator, which is what a key needs.
    void Quoted(const char *value);
    void RawChar(char value);
    void Separate();

    std::span<char> into_;
    std::size_t size_{};
    bool ok_{true};
    // A comma belongs between members, not before the first one, and an array resets that.
    bool first_{true};
    int depth_{};
};

// Fixed-size slots, one producer and one consumer, same shape as AcquisitionEventRing. A row
// longer than a slot is a counted drop rather than a truncated line, which is the same policy
// RowBuilder applies to a row that did not fit its buffer.
template <std::size_t RowBytes, std::size_t Capacity> class EventRowRing {
  public:
    bool Push(std::span<const char> row) {
        if (row.empty() || row.size() > RowBytes) {
            drops_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        const auto head = head_.load(std::memory_order_relaxed);
        if (head - tail_.load(std::memory_order_acquire) == Capacity) {
            drops_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        Slot &slot = slots_[head % Capacity];
        std::memcpy(slot.bytes, row.data(), row.size());
        slot.length = static_cast<std::uint32_t>(row.size());
        head_.store(head + 1, std::memory_order_release);
        return true;
    }
    bool Pop(std::span<char> into, std::size_t &length) {
        length = 0;
        const auto tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire))
            return false;
        const Slot &slot = slots_[tail % Capacity];
        if (slot.length > into.size()) {
            // The consumer's buffer is smaller than a slot, which is a programming error rather
            // than a runtime condition. Drop it rather than overrun, and count it.
            tail_.store(tail + 1, std::memory_order_release);
            drops_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        std::memcpy(into.data(), slot.bytes, slot.length);
        length = slot.length;
        tail_.store(tail + 1, std::memory_order_release);
        return true;
    }
    std::uint64_t Drops() const { return drops_.load(std::memory_order_relaxed); }
    static constexpr std::size_t RowCapacity() { return RowBytes; }

  private:
    static_assert(Capacity > 0);
    struct Slot {
        std::uint32_t length{};
        char bytes[RowBytes]{};
    };
    std::array<Slot, Capacity> slots_{};
    std::atomic<std::uint64_t> head_{}, tail_{}, drops_{};
};

struct EventLogStats {
    std::uint64_t records{};
    std::uint64_t bytes{};
    // Rows the caller could not build or the writer could not take. Counted rather than hidden,
    // because a log missing rows silently would make every count computed from it wrong.
    std::uint64_t dropped{};
    std::uint64_t flushes{};
};

class SIGNALCORE_API EventLog {
  public:
    EventLog() = default;
    ~EventLog();
    EventLog(const EventLog &) = delete;
    EventLog &operator=(const EventLog &) = delete;

    // flush_interval is the ceiling on how much a kill can cost, which 4.8 puts at one second.
    // A non-positive interval flushes every record, which is for tests rather than for a run.
    Status Open(const char *path, Clock clock, Time flush_interval);

    // Appends one record and a newline. A record that did not build is counted and not written.
    Status Write(const RowBuilder &row);
    Status Write(std::span<const char> line);

    // Called at whatever cadence the owner has. Flushes if the interval has elapsed, so the flush
    // is on the second rather than on the frame.
    Status Poll();
    Status Flush();
    Status Close();

    bool IsOpen() const { return file_ != nullptr; }
    const EventLogStats &Stats() const { return stats_; }

  private:
    std::FILE *file_{};
    Clock clock_{};
    Time interval_{};
    Time last_flush_{};
    EventLogStats stats_{};
};

} // namespace signal_core
