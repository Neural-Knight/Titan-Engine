#pragma once

#include <atomic>
#include <cstddef>
#include <memory>

namespace titan {

// Lock-free SPSC ring buffer, fixed preallocated slab -- no new/delete
// per enqueue/dequeue. One slot stays empty so full != empty (usable cap = Capacity - 1).
template <typename T, size_t Capacity>
class SpscQueue {
    static_assert(Capacity >= 2 && (Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two >= 2");

public:
    SpscQueue() : buffer_(std::make_unique<T[]>(Capacity)) {}

    // False if full -- caller decides whether to spin, back off, or drop.
    bool try_enqueue(T&& item)
    {
        const size_t tail = tail_.load(std::memory_order_relaxed);
        const size_t next = advance(tail);
        if (next == head_.load(std::memory_order_acquire))
            return false;
        buffer_[tail] = std::move(item);
        tail_.store(next, std::memory_order_release);  // publishes slot to the consumer
        return true;
    }

    bool try_dequeue(T& out)
    {
        const size_t head = head_.load(std::memory_order_relaxed);
        if (head == tail_.load(std::memory_order_acquire))  // acquire pairs with enqueue's release
            return false;
        out = std::move(buffer_[head]);
        head_.store(advance(head), std::memory_order_release);  // frees slot for the producer to reuse
        return true;
    }

    static constexpr size_t capacity() { return Capacity - 1; }

    bool empty() const { return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire); }

    bool full() const
    {
        return advance(tail_.load(std::memory_order_acquire)) == head_.load(std::memory_order_acquire);
    }

    // Approximate under concurrent access (mixes two independent loads) --
    // exact once producer/consumer are both idle, e.g. after stop()/join().
    size_t size() const
    {
        const size_t head = head_.load(std::memory_order_acquire);
        const size_t tail = tail_.load(std::memory_order_acquire);
        return (tail - head) & kMask;
    }

private:
    static constexpr size_t kMask = Capacity - 1;
    static constexpr size_t advance(size_t index) { return (index + 1) & kMask; }

    std::unique_ptr<T[]> buffer_;
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};
};

}  // namespace titan
