#ifndef __STACK__
#define __STACK__
template <typename T>
class Stack {
    T* head = nullptr;

   public:
    void push(T* item) {
        item->next = head;
        head = item;
    }

    T* pop() {
        if (!head) return nullptr;
        T* item = head;
        head = head->next;
        return item;
    }

    bool empty() const {
        return head == nullptr;
    }

    T* front() const {
        return head;
    }
};

#endif /* stack.h */
