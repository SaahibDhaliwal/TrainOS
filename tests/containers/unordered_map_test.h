#include "test_utils.h"
#include "unordered_map.h"

void basicUnorderedMapTest() {
    // Create a map with capacity for 10 entries
    UnorderedMap<uint64_t, uint64_t, 10> map;

    // Initially empty
    ASSERT(map.size() == 0);
    ASSERT(map.empty());
    ASSERT(!map.full());

    // Insert a single key/value
    ASSERT(map.insert(1, 100));
    ASSERT(map.size() == 1);
    ASSERT(!map.empty());
    ASSERT(!map.full());
    ASSERT(map.exists(1));
    {
        uint64_t* p = map.get(1);
        ASSERT(p && *p == 100);
    }

    // Overwrite existing key
    ASSERT(map.insert(1, 200));
    ASSERT(map.size() == 1);
    {
        uint64_t* p = map.get(1);
        ASSERT(p && *p == 200);
    }

    // Insert multiple distinct keys
    for (uint64_t k : {2, 3, 4, 5}) {
        ASSERT(map.insert(k, k * 10));
    }
    ASSERT(map.size() == 5);
    for (uint64_t k : {2, 3, 4, 5}) {
        ASSERT(map.exists(k));
        uint64_t* p = map.get(k);
        ASSERT(p && *p == k * 10);
    }

    // Erase a middle key
    ASSERT(map.erase(3));
    ASSERT(!map.exists(3));
    ASSERT(map.size() == 4);
    ASSERT(map.get(3) == nullptr);

    // Erase non-existent key should be no-op
    ASSERT(!map.erase(999));
    ASSERT(map.size() == 4);

    // Fill to capacity
    for (uint64_t k = 6; k <= 10; ++k) {
        ASSERT(map.insert(k, k + 100));
    }
    ASSERT(map.size() == 9);  // we inserted keys 1,2,4,5,6,7,8,9,10
    ASSERT(!map.full());
    ASSERT(map.insert(11, 1111));
    ASSERT(map.full());
    ASSERT(map.size() == 10);

    // Insertion should now fail when full
    ASSERT(!map.insert(12, 1200));
    ASSERT(map.size() == 10);

    // Erase all
    for (uint64_t k : {1, 2, 4, 5, 6, 7, 8, 9, 10, 11}) {
        ASSERT(map.exists(k), "the key %d does not exist", k);
    }

    for (uint64_t k : {1, 2, 4, 5, 6, 7, 8, 9, 10, 11}) {
        ASSERT(map.erase(k), "was not able to erase %d", k);
    }

    ASSERT(map.empty());
    ASSERT(map.size() == 0);
    ASSERT(!map.full());
}

void runUnorderedMapTest() {
    basicUnorderedMapTest();
}
