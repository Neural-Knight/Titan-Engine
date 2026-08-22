#include <gtest/gtest.h>

#include <unordered_set>

#include "titan/core/memory_pool.hpp"
#include "titan/core/types.hpp"

using namespace titan;

TEST(MemoryPool, AcquireReturnsDistinctPointers)
{
    FixedObjectPool<Order> pool(4);
    Order* a = pool.acquire();
    Order* b = pool.acquire();
    EXPECT_NE(a, b);
}

TEST(MemoryPool, ExhaustionGrowsANewSlab)
{
    FixedObjectPool<Order> pool(4);
    EXPECT_EQ(pool.slabCount(), 0u);

    std::unordered_set<Order*> seen;
    for (int i = 0; i < 5; ++i)
        seen.insert(pool.acquire());

    EXPECT_EQ(pool.slabCount(), 2u);  // 4 exhausted the first slab, the 5th grew a second
    EXPECT_EQ(seen.size(), 5u);       // all distinct, none aliased across the slab boundary
}

TEST(MemoryPool, ReleaseThenAcquireReusesTheSlot)
{
    FixedObjectPool<Order> pool(4);
    Order* a = pool.acquire();
    pool.release(a);

    Order* b = pool.acquire();
    EXPECT_EQ(a, b);  // freed slot is the natural one to reuse (LIFO free list)
}

TEST(MemoryPool, DoubleReleaseDoesNotCorruptOtherLiveObjects)
{
    FixedObjectPool<Order> pool(4);
    Order* a = pool.acquire();
    Order* b = pool.acquire();

    pool.release(a);
    pool.release(a);  // second release on the same pointer must be a no-op

    Order* c = pool.acquire();
    Order* d = pool.acquire();

    EXPECT_NE(c, d);  // if the double-release pushed `a` twice, these would alias
    EXPECT_NE(c, b);
    EXPECT_NE(d, b);
}

TEST(MemoryPool, AcquiredObjectIsAssignableAndReadableAfterPlacementConstruction)
{
    FixedObjectPool<Order> pool(4);
    Order* order = pool.acquire();
    *order = Order{42, Side::Buy, 100, 10};

    EXPECT_EQ(order->id, 42u);
    EXPECT_EQ(order->quantity, 10u);
}
