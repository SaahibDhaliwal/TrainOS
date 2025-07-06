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

    class Iterator {
        RingBuffer* rb;
        size_t pos;
        size_t remaining;

       public:
        Iterator(RingBuffer* rb, size_t pos, size_t remaining) : rb(rb), pos(pos), remaining(remaining) {
        }

        T& operator*() const {
            return rb->buffer[pos];
        }

        Iterator& operator++() {
            pos = (pos + 1) % CAPACITY;
            --remaining;
            return *this;
        }

        Iterator operator++(int) {
            Iterator tmp = *this;
            ++*this;
            return tmp;
        }

        bool operator==(const Iterator& other) const {
            return remaining == other.remaining;
        }

        bool operator!=(const Iterator& other) const {
            return remaining != other.remaining;
        }

        friend class RingBuffer;
    };

    Iterator begin() {
        return Iterator(this, head, count);
    }

    Iterator end() {
        return Iterator(this, tail, 0);
    }
};

#endif /* ring_buffer.h */
