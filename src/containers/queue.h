#ifndef __QUEUE__
#define __QUEUE__
template <typename T>
class Queue {
   private:
    T* head = nullptr;
    T* tail = nullptr;
    int count = 0;

   public:
    void push(T* node) {
        node->next = nullptr;
        if (!head) { //first one in queue
            head = tail = node;
        } else {
            tail->next = node;
            tail = node;
        }
        count++;
    }

    T* pop() {
        if (!head) return nullptr; //nothing in q
        T* node = head;
        head = head->next;
        if (!head) tail = nullptr; //nothing next
        return node;
    }

    T* front() const {
        return head;
    }

    bool empty() const {
        return head == nullptr;
    }

    int tally() const {
        return count;
    }
};

#endif /* queue.h */
