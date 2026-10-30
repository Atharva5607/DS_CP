// Self-balancing BST (AVL). Keeps machines ordered by exposure score so that
// "which machines have risk in [lo, hi]?" is answered in O(log n + k).
// key = risk * 1'000'000 + machineId  (unique + ordered by risk first)
#pragma once
#include <vector>
#include <algorithm>

class AVLTree {
public:
    AVLTree() = default;
    AVLTree(const AVLTree&) = delete;
    AVLTree& operator=(const AVLTree&) = delete;
    ~AVLTree() { destroy(root_); }

    void insert(long long key, int val) { root_ = ins(root_, key, val); }
    void erase(long long key) { root_ = del(root_, key); }
    void clear() { destroy(root_); root_ = nullptr; size_ = 0; }
    size_t size() const { return size_; }
    int height() const { return H(root_); }

    // values with lo <= key <= hi, in ascending key order
    void range(long long lo, long long hi, std::vector<int>& out) const { rng(root_, lo, hi, out); }
    void inorder(std::vector<int>& out) const { rng(root_, -(1LL << 62), (1LL << 62), out); }

private:
    struct N { long long key; int val; int h; N* l; N* r; };
    N* root_ = nullptr;
    size_t size_ = 0;

    static int H(N* n) { return n ? n->h : 0; }
    static void upd(N* n) { n->h = 1 + std::max(H(n->l), H(n->r)); }
    static int bal(N* n) { return n ? H(n->l) - H(n->r) : 0; }
    static N* rotR(N* y) { N* x = y->l; y->l = x->r; x->r = y; upd(y); upd(x); return x; }
    static N* rotL(N* x) { N* y = x->r; x->r = y->l; y->l = x; upd(x); upd(y); return y; }
    static N* fix(N* n) {
        upd(n);
        int b = bal(n);
        if (b > 1)  { if (bal(n->l) < 0) n->l = rotL(n->l); return rotR(n); }
        if (b < -1) { if (bal(n->r) > 0) n->r = rotR(n->r); return rotL(n); }
        return n;
    }
    N* ins(N* n, long long k, int v) {
        if (!n) { ++size_; return new N{k, v, 1, nullptr, nullptr}; }
        if (k < n->key) n->l = ins(n->l, k, v);
        else if (k > n->key) n->r = ins(n->r, k, v);
        else { n->val = v; return n; }
        return fix(n);
    }
    N* del(N* n, long long k) {
        if (!n) return nullptr;
        if (k < n->key) n->l = del(n->l, k);
        else if (k > n->key) n->r = del(n->r, k);
        else {
            if (!n->l || !n->r) { N* c = n->l ? n->l : n->r; delete n; --size_; return c; }
            N* m = n->r; while (m->l) m = m->l;
            n->key = m->key; n->val = m->val;
            n->r = del(n->r, m->key);
        }
        return fix(n);
    }
    static void rng(N* n, long long lo, long long hi, std::vector<int>& out) {
        if (!n) return;
        if (lo < n->key) rng(n->l, lo, hi, out);
        if (lo <= n->key && n->key <= hi) out.push_back(n->val);
        if (hi > n->key) rng(n->r, lo, hi, out);
    }
    static void destroy(N* n) { if (!n) return; destroy(n->l); destroy(n->r); delete n; }
};
