#ifndef __DEQUE__
#define __DEQUE__

#include <iterator>

#include "test_utils.h"

// Intrusive singly-linked list: T must have a T* next member

template <typename T>
class Deque {
   private:
    T* head = nullptr;
    T* tail = nullptr;
    int count = 0;

   public:
    class Iterator {
        T* node;

       public:
        explicit Iterator(T* ptr) : node(ptr) {
        }

        T& operator*() const {
            return *node;
        }

        Iterator& operator++() {
            node = node->next;
            return *this;
        }

        Iterator operator++(int) {
            Iterator tmp = *this;
            node = node->next;
            return tmp;
        }

        bool operator==(const Iterator& other) const {
            return node == other.node;
        }
        bool operator!=(const Iterator& other) const {
            return node != other.node;
        }

        friend class Deque;
    };

    // Insert node at tail
    void push_back(T* node) {
        node->next = nullptr;
        if (!head) {
            head = tail = node;
        } else {
            tail->next = node;
            tail = node;
        }
        ++count;
    }

    // Remove and return head node
    T* pop_front() {
        if (!head) return nullptr;
        T* node = head;
        head = head->next;
        if (!head) tail = nullptr;
        --count;
        return node;
    }

    T* front() const {
        return head;
    }

    bool empty() const {
        return head == nullptr;
    }

    void reset() {
        head = tail = nullptr;
        count = 0;
    }

    int size() const {
        return count;
    }

    Iterator begin() {
        return Iterator(head);
    }

    Iterator end() {
        return Iterator(nullptr);
    }

    void erase(Iterator pos) {
        if (pos.node == end().node) return;

        T* node = pos.node;

        // unlink
        if (node->prev) {
            node->prev->next = node->next;
        } else {
            head = node->next;
        }

        if (node->next) {
            node->next->prev = node->prev;
        } else {
            tail = node->prev;
        }
        node->next = node->prev = nullptr;
        --count;
    }
};

#endif /* deque.h */
