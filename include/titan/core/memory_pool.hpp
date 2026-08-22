#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <vector>

namespace titan {

// Free-list slab pool. acquire() never fails -- grows by one slab when empty.
// release() on a pointer not live from this pool is a no-op, not a crash.
template <typename T>
class FixedObjectPool {
public:
    explicit FixedObjectPool(size_t slabSize = 4096) : slabSize_(slabSize) {}

    T* acquire()
    {
        if (freeList_.empty())
            growBySlab();
        T* ptr = freeList_.back();
        freeList_.pop_back();
        *liveFlagFor(ptr) = 1;
        new (ptr) T();
        return ptr;
    }

    void release(T* ptr)
    {
        uint8_t* live = liveFlagFor(ptr);
        if (!live || !*live)
            return;
        ptr->~T();
        *live = 0;
        freeList_.push_back(ptr);
    }

    size_t slabCount() const { return slabs_.size(); }
    size_t freeCount() const { return freeList_.size(); }

private:
    using Storage = std::aligned_storage_t<sizeof(T), alignof(T)>;
    struct Slab {
        std::unique_ptr<Storage[]> storage;
        std::vector<uint8_t> live;  // not vector<bool>: need real addresses for liveFlagFor
    };

    void growBySlab()
    {
        Slab slab{std::make_unique<Storage[]>(slabSize_), std::vector<uint8_t>(slabSize_, 0)};
        T* base = reinterpret_cast<T*>(slab.storage.get());
        for (size_t i = 0; i < slabSize_; ++i)
            freeList_.push_back(base + i);
        slabs_.push_back(std::move(slab));
    }

    // nullptr if `ptr` doesn't belong to any slab in this pool.
    uint8_t* liveFlagFor(T* ptr)
    {
        for (Slab& slab : slabs_)
        {
            T* base = reinterpret_cast<T*>(slab.storage.get());
            if (ptr >= base && ptr < base + slabSize_)
                return &slab.live[static_cast<size_t>(ptr - base)];
        }
        return nullptr;
    }

    size_t slabSize_;
    std::vector<Slab> slabs_;
    std::vector<T*> freeList_;
};

}  // namespace titan
