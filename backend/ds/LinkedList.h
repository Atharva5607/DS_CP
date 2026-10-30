// Singly linked list (custom, no STL containers). Used for adjacency lists,
// hash-table buckets, the incident log, and as the base of Stack and Queue.
#pragma once
#include <cstddef>

template <typename T>
class LinkedList {
public:
    struct Node {
        T val;
        Node* next;
        explicit Node(const T& v) : val(v), next(nullptr) {}
    };

    LinkedList() = default;
    LinkedList(const LinkedList& o) { for (Node* p = o.head_; p; p = p->next) push_back(p->val); }
    LinkedList(LinkedList&& o) noexcept : head_(o.head_), tail_(o.tail_), size_(o.size_) {
        o.head_ = o.tail_ = nullptr; o.size_ = 0;
    }
    LinkedList& operator=(const LinkedList& o) {
        if (this != &o) { clear(); for (Node* p = o.head_; p; p = p->next) push_back(p->val); }
        return *this;
    }
    LinkedList& operator=(LinkedList&& o) noexcept {
        if (this != &o) {
            clear();
            head_ = o.head_; tail_ = o.tail_; size_ = o.size_;
            o.head_ = o.tail_ = nullptr; o.size_ = 0;
        }
        return *this;
    }
    ~LinkedList() { clear(); }

    void push_back(const T& v) {                    // O(1)
        Node* n = new Node(v);
        if (tail_) tail_->next = n; else head_ = n;
        tail_ = n; ++size_;
    }
    void push_front(const T& v) {                   // O(1)
        Node* n = new Node(v);
        n->next = head_; head_ = n;
        if (!tail_) tail_ = n;
        ++size_;
    }
    bool pop_front(T& out) {                        // O(1)
        if (!head_) return false;
        Node* n = head_; out = n->val; head_ = n->next;
        if (!head_) tail_ = nullptr;
        delete n; --size_;
        return true;
    }
    bool peek_front(T& out) const {
        if (!head_) return false;
        out = head_->val; return true;
    }
    template <typename Pred>
    bool remove_first(Pred pred) {                  // O(n)
        Node* prev = nullptr;
        for (Node* p = head_; p; prev = p, p = p->next) {
            if (!pred(p->val)) continue;
            if (prev) prev->next = p->next; else head_ = p->next;
            if (p == tail_) tail_ = prev;
            delete p; --size_;
            return true;
        }
        return false;
    }
    void clear() {
        while (head_) { Node* n = head_; head_ = n->next; delete n; }
        tail_ = nullptr; size_ = 0;
    }
    size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
    Node* head() const { return head_; }            // manual iteration (used by iterative DFS)

    struct Iterator {
        Node* p;
        T& operator*() const { return p->val; }
        Iterator& operator++() { p = p->next; return *this; }
        bool operator!=(const Iterator& o) const { return p != o.p; }
    };
    Iterator begin() const { return {head_}; }
    Iterator end() const { return {nullptr}; }

private:
    Node* head_ = nullptr;
    Node* tail_ = nullptr;
    size_t size_ = 0;
};
