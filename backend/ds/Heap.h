// Binary heap (array based). `Before(a,b)` returns true when a must leave the
// heap before b. Used as: min-heap inside Dijkstra and Kruskal, and as the
// max-heap that ranks cyber incidents by severity.
#pragma once
#include <vector>
#include <utility>

template <typename T, typename Before>
class Heap {
public:
    void push(const T& v) {                         // O(log n)
        a_.push_back(v);
        size_t i = a_.size() - 1;
        while (i > 0) {
            size_t p = (i - 1) / 2;
            if (!before_(a_[i], a_[p])) break;
            std::swap(a_[i], a_[p]); i = p;
        }
    }
    bool pop(T& out) {                              // O(log n)
        if (a_.empty()) return false;
        out = a_[0];
        a_[0] = a_.back(); a_.pop_back();
        size_t i = 0, n = a_.size();
        while (true) {
            size_t l = 2 * i + 1, r = l + 1, best = i;
            if (l < n && before_(a_[l], a_[best])) best = l;
            if (r < n && before_(a_[r], a_[best])) best = r;
            if (best == i) break;
            std::swap(a_[i], a_[best]); i = best;
        }
        return true;
    }
    bool peek(T& out) const { if (a_.empty()) return false; out = a_[0]; return true; }
    bool empty() const { return a_.empty(); }
    size_t size() const { return a_.size(); }
    void clear() { a_.clear(); }
private:
    std::vector<T> a_;
    Before before_;
};
