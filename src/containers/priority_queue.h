#ifndef __PRIORITY_QUEUE__
#define __PRIORITY_QUEUE__
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>

#include "test_utils.h"

template <typename T, size_t CAPACITY, class Compare = std::greater<T>>
class PriorityQueue {
    T data[CAPACITY];
    size_t count;
    Compare comp;

    uint64_t parent(uint64_t idx) {
        return (idx - 1) / 2;
    }

    uint64_t leftChild(uint64_t idx) {
        return (2 * idx) + 1;
    }

    uint64_t rightChild(uint64_t idx) {
        return (2 * idx) + 2;
    }

    void fixUp(uint64_t idx) {
        while (idx > 0 && comp(data[parent(idx)], data[idx])) {
            std::swap(data[parent(idx)], data[idx]);
            idx = parent(idx);
        }
    }

    void fixDown(uint64_t idx) {
        while (true) {
            uint64_t l = leftChild(idx);
            if (l >= count) break;  // no children

            // pick the smaller child
            uint64_t smaller = l;
            uint64_t r = rightChild(idx);
            if (r < count && comp(data[l], data[r])) {
                smaller = r;
            }

            if (!comp(data[idx], data[smaller])) {  // heap property holds
                break;
            }

            std::swap(data[idx], data[smaller]);
            idx = smaller;
        }
    }

   public:
    PriorityQueue() : count(0), comp(Compare()) {
    }

    bool push(const T& value) {
        if (full()) return false;
        data[count] = value;
        fixUp(count);
        count++;

        return true;
    }

    T pop() {
        ASSERT(!empty());
        T value = data[0];
        count--;
        data[0] = data[count];
        fixDown(0);
        return value;
    }

    T top() {
        ASSERT(!empty());
        return data[0];
    }

    size_t size() const {
        return count;
    }

    bool empty() const {
        return count == 0;
    }

    bool full() const {
        return count == CAPACITY;
    }
};

#endif /* priority_queue.h */
