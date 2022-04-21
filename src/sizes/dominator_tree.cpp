// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#include <queue>
#include "dominator_tree.h"

namespace {
    using jlongs = std::vector<jlong>;
    using graph_t = std::vector<jlongs>;

    void link(jlong v, jlong w, jlongs &ancestor) {
        ancestor[w] = v;
    }

    void compress(jlong v, jlongs &ancestor, jlongs &label, jlongs &semi) {
        if (ancestor[ancestor[v]] != -1) {
            compress(ancestor[v], ancestor, label, semi);
            if (semi[label[ancestor[v]]] < semi[label[v]]) {
                label[v] = label[ancestor[v]];
            }
            ancestor[v] = ancestor[ancestor[v]];
        }
    }

    jlong eval(jlong v, jlongs &ancestor, jlongs &label, jlongs &semi) {
        if (ancestor[v] == -1) {
            return v;
        }
        compress(v, ancestor, label, semi);
        
        return label[v];
    }

    void dfs(jlong v, jlong &n, const graph_t &graph, jlongs &semi,
             jlongs &parent, jlongs &vertex, graph_t &pred) {
        semi[v] = n;
        vertex[n] = v;
        n++;
        for (jlong w : graph[v]) {
            if (w != 0 && semi[w] == 0) {
                parent[w] = v;
                dfs(w, n, graph, semi, parent, vertex, pred);
            }
            pred[w].push_back(v);
        }
    }
}

jlongs calculateRetainedSizesViaDominatorTree(const graph_t &graph, const jlongs &sizes) {
    auto n = static_cast<jlong>(graph.size());
    jlongs semi(n);
    jlongs parent(n);
    jlongs vertex(n);
    jlongs ancestor(n);
    jlongs label(n);
    jlongs dom(n);
    jlongs retainedSizes(n);
    graph_t pred(n);
    graph_t bucket(n);

    for (jlong i = 0; i < n; i++) {
        retainedSizes[i] = sizes[i];
        label[i] = i;
        ancestor[i] = -1;
    }

    n = 0;
    dfs(0, n, graph, semi, parent, vertex, pred);

    for (jlong i = n - 1; i > 0; i--) {
        jlong w = vertex[i];
        for (jlong v : pred[w]) {
            jlong u = eval(v, ancestor, label, semi);
            if (semi[u] < semi[w]) {
                semi[w] = semi[u];
            }
        }
        bucket[vertex[semi[w]]].push_back(w);
        link(parent[w], w, ancestor);

        for (jlong v : bucket[parent[w]]) {
            jlong u = eval(v, ancestor, label, semi);
            if (semi[u] < semi[v]) {
                dom[v] = u;
            } else {
                dom[v] = parent[w];
            }
        }
        bucket[parent[w]].clear();
    }

    dom[0] = -1;
    jlongs childCount(n);
    for (jlong i = 1; i < n; i++) {
        jlong w = vertex[i];
        if (dom[w] != vertex[semi[w]]) {
            dom[w] = dom[dom[w]];
        }

        if (dom[w] >= 0) {
            childCount[dom[w]]++;
        }
    }

    std::queue<jlong> leaves;
    for (jlong i = 1; i < n; i++) {
        if (childCount[i] == 0) {
            leaves.push(i);
        }
    }

    while (!leaves.empty()) {
        jlong leaf = leaves.front();
        leaves.pop();
        jlong d = dom[leaf];
        if (d >= 0) {
            retainedSizes[d] += retainedSizes[leaf];
            if (--childCount[d] == 0) {
                leaves.push(d);
            }
        }
    }

    return retainedSizes;
}
