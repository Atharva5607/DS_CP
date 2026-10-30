// FIFO queue on top of LinkedList. Used for: BFS frontier (attack waves)
// and the component search after isolating machines.
#pragma once
#include "LinkedList.h"

template <typename T>
class Queue {
public:
    void push(const T& v) { list_.push_back(v); }
    bool pop(T& out) { return list_.pop_front(out); }
    bool empty() const { return list_.empty(); }
    size_t size() const { return list_.size(); }
    void clear() { list_.clear(); }
private:
    LinkedList<T> list_;
};
