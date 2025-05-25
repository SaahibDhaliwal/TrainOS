#ifndef __QUEUE__
#define __QUEUE__
template <typename T>
class Queue {
   private:
    T* head = nullptr;
    T* tail = nullptr;

   public:
    void push(T* node) {
        node->next = nullptr;
        if (!head) {
            head = tail = node;
        } else {
            tail->next = node;
            tail = node;
        }
    }

    T* pop() {
        if (!head) return nullptr;
        T* node = head;
        head = head->next;
        if (!head) tail = nullptr;
        return node;
    }

    T* front() const {
        return head;
    }

    bool empty() const {
        return head == nullptr;
    }
};

#endif /* queue.h */
