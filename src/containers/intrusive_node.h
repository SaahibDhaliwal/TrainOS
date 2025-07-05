#ifndef __INTRUSIVE_NODE__
#define __INTRUSIVE_NODE__

template <typename T>
class IntrusiveNode {
    T* next = nullptr;
    T* prev = nullptr;

    template <typename U>
    friend class Queue;
    template <typename U>
    friend class Stack;
    template <typename U>
    friend class Deque;
};

#endif /* intrusive_node.h */