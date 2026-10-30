// Hash table with separate chaining (buckets are LinkedLists) and automatic
// rehashing. Maps machine names ("SRV-4") to node ids in O(1) average.
#pragma once
#include <string>
#include <vector>
#include "LinkedList.h"

template <typename V>
class HashTable {
public:
    explicit HashTable(size_t cap = 16) : buckets_(cap) {}

    void put(const std::string& key, const V& val) {
        if (size_ + 1 > buckets_.size() * 3 / 4) rehash(buckets_.size() * 2);
        for (auto& e : buckets_[idx(key)]) if (e.key == key) { e.val = val; return; }
        buckets_[idx(key)].push_back(Entry{key, val});
        ++size_;
    }
    V* get(const std::string& key) {
        for (auto& e : buckets_[idx(key)]) if (e.key == key) return &e.val;
        return nullptr;
    }
    bool erase(const std::string& key) {
        bool ok = buckets_[idx(key)].remove_first([&](const Entry& e) { return e.key == key; });
        if (ok) --size_;
        return ok;
    }
    void clear() { for (auto& b : buckets_) b.clear(); size_ = 0; }
    size_t size() const { return size_; }
    size_t bucketCount() const { return buckets_.size(); }

private:
    struct Entry { std::string key; V val; };
    static size_t hashOf(const std::string& s) {     // djb2
        size_t h = 5381;
        for (unsigned char c : s) h = h * 33 + c;
        return h;
    }
    size_t idx(const std::string& k) const { return hashOf(k) % buckets_.size(); }
    void rehash(size_t newCap) {
        std::vector<LinkedList<Entry>> old(newCap);
        old.swap(buckets_);
        for (auto& b : old) for (auto& e : b) buckets_[idx(e.key)].push_back(e);
    }
    std::vector<LinkedList<Entry>> buckets_;
    size_t size_ = 0;
};
