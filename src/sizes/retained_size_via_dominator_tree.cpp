// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#include "retained_size_via_dominator_tree.h"

using graph_t = std::vector<std::vector<jlong>>;

namespace {
    void link(jlong v, jlong w, std::vector<jlong> &ancestor) {
        ancestor[w] = v;
    }

    void compress(jlong v, std::vector<jlong> &ancestor, std::vector<jlong> &label, std::vector<jlong> &semi) {
        if (ancestor[ancestor[v]] != 0) {
            compress(ancestor[v], ancestor, label, semi);
            if (semi[label[ancestor[v]]] < semi[label[v]]) {
                label[v] = label[ancestor[v]];
            }
            ancestor[v] = ancestor[ancestor[v]];
        }
    }

    jlong eval(jlong v, std::vector<jlong> &ancestor, std::vector<jlong> &label, std::vector<jlong> &semi) {
        if (ancestor[v] == 0) {
            return v;
        }
        compress(v, ancestor, label, semi);
        return label[v];
    }

    void dfs(
        jlong v, jlong &n,
        const graph_t &graph,
        std::vector<jlong> &semi,
        std::vector<jlong> &parent,
        std::vector<jlong> &vertex,
        graph_t &pred
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

    std::vector<jlong> calculateRetainedSizes(const graph_t &graph, const std::vector<jlong> &sizes) {
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

        for (jlong i = 0; i < numOfVertices; i++) {
            retained_sizes[i] = sizes[i];
            label[i] = i;
        }

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
        for (jlong i = 1; i < n; i++) {
            jlong w = vertex[i];
            if (dom[w] != vertex[semi[w]]) {
                dom[w] = dom[dom[w]];
            }

            jlong temp = dom[w];
            while (temp != -1) {
                retained_sizes[temp] += sizes[w];
                temp = dom[temp];
            }
        }

        return retained_sizes;
    }
}

struct HeapDumpInfo {
    HeapDumpInfo() : graph(1), sizes(1) {}

    graph_t graph;
    std::vector<jlong> sizes;
    std::vector<jlong> roots;
    jlong tag = 1;
};

jint JNICALL dumpHeapGraph(jvmtiHeapReferenceKind refKind, const jvmtiHeapReferenceInfo *refInfo, jlong classTag,
                           jlong referrerClassTag, jlong size, jlong *tagPtr,
                           jlong *referrerTagPtr, jint length, void *userData) {
    auto *info = reinterpret_cast<HeapDumpInfo *>(userData);

    if (*tagPtr == 0) {
        *tagPtr = info->tag;
        info->sizes.push_back(size);
        info->graph.emplace_back();
        info->tag++;
    }

    if (referrerTagPtr == nullptr) {
        info->roots.push_back(*tagPtr);
    } else {
        info->graph[*referrerTagPtr].push_back(*tagPtr);
    }

    return JVMTI_VISIT_OBJECTS;
}

jlongArray RetainedSizeViaDominatorTreeAction::executeOperation(jobjectArray objects) {
    HeapDumpInfo info;
    progressManager.updateProgress(10, "Capturing heap dump");
    jvmtiError err = FollowReferences(0, nullptr, nullptr, dumpHeapGraph, &info, "dump heap graph");
    if (err != JVMTI_ERROR_NONE) return nullptr;

    for (jlong root : info.roots) {
        info.graph[0].push_back(root);
    }

    progressManager.updateProgress(10, "Calculating retained size");
    std::vector<jlong> retained_sizes = calculateRetainedSizes(info.graph, info.sizes);

    progressManager.updateProgress(10, "Extracting answer");
    std::vector<jlong> result;
    jsize size = env->GetArrayLength(objects);
    result.reserve(size);
    for (jsize i = 0; i < size; i++) {
        jobject object = env->GetObjectArrayElement(objects, i);
        jlong tag;
        jvmti->GetTag(object, &tag);
        result.push_back(retained_sizes[tag]);
    }

    return toJavaArray(env, result);
}

jvmtiError RetainedSizeViaDominatorTreeAction::cleanHeap() {
    return removeAllTagsFromHeap(jvmti, nullptr);
}

RetainedSizeViaDominatorTreeAction::RetainedSizeViaDominatorTreeAction(JNIEnv *env, jvmtiEnv *jvmti, jobject object)
        : MemoryAgentAction(env, jvmti, object) {

}
