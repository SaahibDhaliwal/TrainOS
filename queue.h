#ifndef __QUEUE__
#define __QUEUE__

#include <cstddef>

template <typename T, std::size_t CAPACITY>
class Queue {
    T _buffer[CAPACITY];
    int _head;
    int _tail;
    int _size;

   public:
    Queue() : _head(0), _tail(0), _size(0) {};

    bool front(T* out) {
        if (empty()) return false;
        *out = _buffer[_head];
        return true;
    }

    bool push(T command) {
        if (_size == CAPACITY) return false;
        _buffer[_tail] = command;
        _tail = (_tail + 1) % CAPACITY;
        ++_size;
        return true;
    }

    bool pop() {
        if (empty()) return false;
        _head = (_head + 1) % CAPACITY;
        _size -= 1;
        return true;
    }

    int size() {
        return _size;
    }

    bool empty() {
        return _size == 0;
    }
};

#endif
