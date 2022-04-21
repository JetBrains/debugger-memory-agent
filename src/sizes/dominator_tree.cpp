// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#include <queue>
#include "dominator_tree.h"

namespace {
    void link(long v, long w, std::vector<long> &ancestor) {
        ancestor[w] = v;
    }

    void compress(long v, std::vector<long> &ancestor, std::vector<long> &label, std::vector<long> &semi) {
        if (ancestor[ancestor[v]] != -1) {
            compress(ancestor[v], ancestor, label, semi);
            if (semi[label[ancestor[v]]] < semi[label[v]]) {
                label[v] = label[ancestor[v]];
            }
            ancestor[v] = ancestor[ancestor[v]];
        }
    }

    long eval(long v, std::vector<long> &ancestor, std::vector<long> &label, std::vector<long> &semi) {
        if (ancestor[v] == -1) {
            return v;
        }
        compress(v, ancestor, label, semi);
        
        return label[v];
    }

    void dfs(
        jlong v, jlong &n,
        const std::vector<std::vector<jlong>> &graph,
        std::vector <jlong> &semi,
        std::vector <jlong> &parent,
        std::vector <jlong> &vertex,
        std::vector<std::vector<jlong>> &pred
    ) {
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

std::vector<jlong> calculateRetainedSizes(const std::vector<std::vector<jlong>> &graph, const std::vector<jlong> &sizes) {
    size_t numOfVertices = graph.size();

    std::vector<jlong> semi(numOfVertices);
    std::vector<jlong> parent(numOfVertices);
    std::vector<jlong> vertex(numOfVertices);
    std::vector<std::vector<jlong>> pred(numOfVertices);
    std::vector<std::vector<jlong>> bucket(numOfVertices);
    std::vector<jlong> ancestor(numOfVertices);
    std::vector<jlong> label(numOfVertices);
    std::vector<jlong> dom(numOfVertices);
    std::vector<jlong> retained_sizes(numOfVertices);
    std::vector<jlong> child(numOfVertices);
    std::vector<jlong> link_size(numOfVertices);

    for (jlong i = 0; i < numOfVertices; i++) {
        retained_sizes[i] = sizes[i];
        label[i] = i;
        link_size[i] = 1;
        ancestor[i] = -1;
    }

    link_size[0] = 0;
    label[0] = 0;
    semi[0] = 0;

    jlong n = 0;
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
    std::vector<jlong> childCount(numOfVertices);
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
            retained_sizes[d] += retained_sizes[leaf];
            if (--childCount[d] == 0) {
                leaves.push(d);
            }
        }
    }

    return retained_sizes;
}
