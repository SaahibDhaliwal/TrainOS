#include <functional>
#include <utility>

#include "deque.h"
#include "intrusive_node.h"
#include "queue.h"
#include "rpi.h"
template <typename Key, typename Value, size_t MAP_CAPACITY, class Hash = std::hash<Key>,
          class KeyEqual = std::equal_to<Key>>
class UnorderedMap {
    struct EntryNode : IntrusiveNode<EntryNode> {
        size_t index;  // index into keys[]/vals[] arrays
    };

    Key keys[MAP_CAPACITY];
    Value vals[MAP_CAPACITY];

    EntryNode pool[MAP_CAPACITY];
    Queue<EntryNode> freeList;
    Deque<EntryNode> buckets[MAP_CAPACITY];

    size_t sz = 0;
    Hash hasher = Hash();
    KeyEqual equal = KeyEqual();

   public:
    UnorderedMap() {
        // initially all slots are free
        for (size_t i = 0; i < MAP_CAPACITY; ++i) {
            pool[i].index = i;
            freeList.push(&pool[i]);
        }
    }

    bool insert(const Key& k, const Value& v) {
        // overwrite if exists
        if (exists(k)) {
            Value* p = get(k);
            *p = v;
            return true;
        }

        if (freeList.empty()) return false;  // full

        EntryNode* node = freeList.pop();
        size_t slotIdx = node->index;
        keys[slotIdx] = k;
        vals[slotIdx] = v;

        size_t bucketIdx = hasher(k) % MAP_CAPACITY;
        buckets[bucketIdx].push_back(node);
        sz += 1;
        return true;
    }

    bool erase(const Key& k) {
        size_t bucketIdx = hasher(k) % MAP_CAPACITY;
        bool removed = false;

        for (auto it = buckets[bucketIdx].begin(); it != buckets[bucketIdx].end(); ++it) {
            EntryNode* node = &(*it);
            size_t slotIdx = node->index;

            if (equal(keys[slotIdx], k)) {
                buckets[bucketIdx].erase(it);  // erase first, invalid pointers if we push to freeList first
                freeList.push(node);
                sz -= 1;
                removed = true;
                break;
            }
        }

        return removed;
    }

    bool exists(const Key& k) {
        return get(k) != nullptr;
    }

    Value* get(const Key& k) {
        size_t bucketIdx = hasher(k) % MAP_CAPACITY;

        for (auto it = buckets[bucketIdx].begin(); it != buckets[bucketIdx].end(); ++it) {
            EntryNode* node = &(*it);
            if (equal(keys[node->index], k)) {
                return &vals[node->index];
            }
        }

        return nullptr;
    }

    size_t size() const {
        return sz;
    }

    bool empty() const {
        return sz == 0;
    }

    bool full() const {
        return sz == MAP_CAPACITY;
    }

    Value& operator[](const Key& k) {
        if (Value* p = get(k)) {
            return *p;
        }

        insert(k, Value());  // inserts a 0 for ints
        return *get(k);      // returns a reference to the slot
    }
};
