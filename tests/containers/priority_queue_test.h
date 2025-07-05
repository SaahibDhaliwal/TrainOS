#include "priority_queue.h"
#include "test_utils.h"

void findMinTest() {
    PriorityQueue<uint64_t, 100> pq;

    pq.push(10);
    pq.push(1000);
    pq.push(500);
    pq.push(2);
    pq.push(3);
    pq.push(4);
    pq.push(50);

    ASSERT(pq.top() == 2);
}

void basicTest() {
    PriorityQueue<uint64_t, 100> pq;

    ASSERT(pq.size() == 0);

    pq.push(10);
    ASSERT(pq.top() == 10);
    ASSERT(pq.size() == 1);

    pq.push(5);
    ASSERT(pq.top() == 5);
    ASSERT(pq.size() == 2);

    uint64_t minVal = pq.pop();
    ASSERT(minVal == 5);
    ASSERT(pq.size() == 1);
    ASSERT(pq.top() == 10);

    minVal = pq.pop();
    ASSERT(minVal == 10);
    ASSERT(pq.empty());
}

void runPriorityQueueTest() {
    basicTest();
}
