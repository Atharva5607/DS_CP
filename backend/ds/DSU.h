// Disjoint Set Union (union-find) with path compression + union by rank.
// Used by Kruskal's algorithm to detect cycles in near O(1).
#pragma once
#include <vector>
#include <utility>

class DSU {
public:
    explicit DSU(int n) : p_(n), r_(n, 0) { for (int i = 0; i < n; ++i) p_[i] = i; }
    int find(int x) { return p_[x] == x ? x : p_[x] = find(p_[x]); }
    bool unite(int a, int b) {
        a = find(a); b = find(b);
        if (a == b) return false;
        if (r_[a] < r_[b]) std::swap(a, b);
        p_[b] = a;
        if (r_[a] == r_[b]) ++r_[a];
        return true;
    }
private:
    std::vector<int> p_, r_;
};
