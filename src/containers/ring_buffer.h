#ifndef __RING_BUFFER__
#define __RING_BUFFER__
#include <cstddef>

template <typename T, size_t CAPACITY>
class RingBuffer {
    T buffer[CAPACITY];
    size_t head, tail, count;

   public:
    RingBuffer() : head(0), tail(0), count(0) {
    }

    bool push(const T& value) {
        if (count == CAPACITY) return false;
        buffer[tail] = value;
        tail = (tail + 1) % CAPACITY;
        ++count;
        return true;
    }

    T* pop() {
        if (empty()) return nullptr;
        T* value = &buffer[head];
        head = (head + 1) % CAPACITY;
        --count;
        return value;
    }

    T* front() {
        if (empty()) return nullptr;
        return &buffer[head];
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

#endif /* ring_buffer.h */
