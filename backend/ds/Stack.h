// LIFO stack on top of LinkedList. Used for: iterative DFS (lateral movement),
// and the undo history of defensive actions.
#pragma once
#include "LinkedList.h"

template <typename T>
class Stack {
public:
    void push(const T& v) { list_.push_front(v); }
    bool pop(T& out) { return list_.pop_front(out); }
    bool top(T& out) const { return list_.peek_front(out); }
    bool empty() const { return list_.empty(); }
    size_t size() const { return list_.size(); }
    void clear() { list_.clear(); }
private:
    LinkedList<T> list_;
};
