// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#include <vector>
#include <queue>
#include <unordered_set>

#include "retained_size_via_dominator_tree.h"

namespace {
    void log(const std::string &str) {
        std::ofstream out;
        out.open("/Users/Nikita.Nazarov/CLionProjects/debugger-memory-agent/log.txt", std::ios_base::app);
        out << str << std::endl;
    }

    class Timer {
    public:
        std::chrono::steady_clock::time_point begin;
        void start() {
            begin = std::chrono::steady_clock::now();
        }

        void end() {
            log(std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin).count()));
        }
    };

    Timer timer;
    class graph {
    public:
        void addNewVertexAndListNeighbours(const std::vector<jlong> &neighbours) {
            for (jlong neighbour : neighbours) {
                vertices.push_back(neighbour);
            }
            jlong last = indices.size() - 1;
            indices.push_back(indices[last] + neighbours.size());
        }

        template<class Iterator>
        void addNewVertexAndListNeighbours(Iterator begin, Iterator end) {
            jlong addedNeighbours = 0;
            for (Iterator it = begin; it  != end; ++it, ++addedNeighbours) {
                vertices.push_back(*it);
            }

            jlong last = indices.size() - 1;
            indices.push_back(indices[last] + addedNeighbours);
        }

        void replaceStartVertexAndListNeighbours(const std::unordered_set<jlong> &neighbours) {
            for (jlong i = 1; i < indices.size(); i++) {
                indices[i] += neighbours.size();
            }

            std::vector<jlong> newVertices;
            jlong neighboursSize = neighbours.size();
            newVertices.resize(neighboursSize + vertices.size());
            jlong i = 0;
            for (auto it = neighbours.begin(); it != neighbours.end(); ++it, ++i) {
                newVertices[i] = *it;
            }

            for (jlong i = 0; i < vertices.size(); i++) {
                newVertices[neighboursSize + i] = vertices[i];
            }

            vertices = std::move(newVertices);
        }

        [[nodiscard]] jlong numOfVertices() const { return indices.size() - 1; }

        void print() {
            std::cout << "Num of vertices: " << numOfVertices() << std::endl;
            for (size_t i = 0; i < numOfVertices(); i++) {
                std::cout << "Neighbors of " << i << ":" << std::endl;
                visitNeighbours(i, [](jlong v) { std::cout << v << " "; });
                std::cout << std::endl;
            }
        }

        void visitNeighbours(jlong vertex, const std::function<void(jlong)> &&callback) const {
            if (vertex < 0 || vertex >= numOfVertices()) {
                return;
            }

            for (jlong i = indices[vertex]; i < indices[vertex + 1]; i++) {
                callback(vertices[i]);
            }
        }

        [[nodiscard]] graph transpose() const {
            graph transposed;
            std::vector<jlong> currentlyAddedVerticesCnt;
            std::vector<jlong> neighboursCnt;
            neighboursCnt.resize(indices.size());
            currentlyAddedVerticesCnt.resize(indices.size());
            transposed.indices.resize(indices.size());
            transposed.vertices.resize(vertices.size());
            for (jlong i = 0; i < indices.size() - 1; i++) {
                for (jlong j = indices[i]; j < indices[i + 1]; j++) {
                    ++neighboursCnt[vertices[j] + 1];
                }
            }

            jlong sum = 0;
            for (jlong i = 0; i < neighboursCnt.size(); i++) {
                sum += neighboursCnt[i];
                transposed.indices[i] = sum;
            }

            for (jlong i = 0; i < indices.size() - 1; i++) {
                for (jlong j = indices[i]; j < indices[i + 1]; j++) {
                    jlong vertex = vertices[j];
                    jlong index = transposed.indices[vertex];
                    transposed.vertices[index + currentlyAddedVerticesCnt[vertex]++] = i;
                }
            }

            return transposed;
        }

        void clear() {
            vertices.clear();
            indices = {0};
        }

    private:
        std::vector<jlong> indices = {0};
        std::vector<jlong> vertices;
    };

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
        jlong &n, const graph &g,
        std::vector<jlong> &semi,
        std::vector<jlong> &parent,
        std::vector<jlong> &vertex
    ) {
        std::stack<jlong> stack;
        stack.push(0);
        n++;
        while (!stack.empty()) {
            jlong t = stack.top();
            stack.pop();
            g.visitNeighbours(t, [&](jlong w) {
                if (w != 0 && semi[w] == 0) {
                    semi[w] = n;
                    vertex[n] = w;
                    n++;
                    parent[w] = t;
                    stack.push(w);
                }
            });
        }
    }

    std::vector<jlong> calculateRetainedSizes(graph &g, const std::vector<jlong> &sizes) {
        size_t numOfVertices = g.numOfVertices();

        log("Allocating memory...");
        std::vector<jlong> semi(numOfVertices);
        std::vector<jlong> parent(numOfVertices);
        std::vector<jlong> vertex(numOfVertices);
        std::vector<std::vector<jlong>> bucket(numOfVertices);
        std::vector<jlong> ancestor(numOfVertices);
        std::vector<jlong> label(numOfVertices);
        std::vector<jlong> dom(numOfVertices);
        std::vector<jlong> retained_sizes(numOfVertices);

        for (jlong i = 0; i < numOfVertices; i++) {
            retained_sizes[i] = sizes[i];
            label[i] = i;
        }

        log("Traversing graph...");
        jlong n = 0;
        dfs(n, g, semi, parent, vertex);

        graph transposed = g.transpose();
        g.clear();

        for (jlong i = n - 1; i > 0; i--) {
            jlong w = vertex[i];
            transposed.visitNeighbours(w, [&](size_t v) {
                jlong u = eval(v, ancestor, label, semi);
                if (semi[u] < semi[w]) {
                    semi[w] = semi[u];
                }
            });

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

        log("Fetching retained size...");
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
}

struct HeapDumpInfo {
    HeapDumpInfo() : g(1), sizes(1) {}

    graph graph;
    std::vector<jlong> sizes;
    std::unordered_set<jlong> roots;
    std::vector<jlong> currentTagNeighbours;
    std::vector<std::vector<jlong>> g;
    jlong currentReferrerTag = 0;
    jlong tag = 1;
};

jint JNICALL dumpHeapGraph(jvmtiHeapReferenceKind refKind, const jvmtiHeapReferenceInfo *refInfo, jlong classTag,
                           jlong referrerClassTag, jlong size, jlong *tagPtr,
                           jlong *referrerTagPtr, jint length, void *userData) {
    if (refKind == JVMTI_HEAP_REFERENCE_CLASS ||
        refKind == JVMTI_HEAP_REFERENCE_CLASS_LOADER ||
        refKind == JVMTI_HEAP_REFERENCE_JNI_GLOBAL ||
        refKind == JVMTI_HEAP_REFERENCE_JNI_LOCAL) {
        return 0;
    }

    auto *info = reinterpret_cast<HeapDumpInfo *>(userData);

    if (*tagPtr == 0) {
        *tagPtr = info->tag;
        info->sizes.push_back(size);
        info->g.emplace_back();
        info->tag++;
    }

    if (referrerTagPtr == nullptr) {
        info->roots.insert(*tagPtr);
    } else {
        info->g[*referrerTagPtr].push_back(*tagPtr);
    }

    return JVMTI_VISIT_OBJECTS;
}

jint JNICALL visitObject(jlong classTag, jlong size, jlong *tagPtr, jint length, void *userData) {
    if (*tagPtr == 0) {
        return JVMTI_ITERATION_CONTINUE;
    }

    return JVMTI_ITERATION_CONTINUE;
}

jlongArray RetainedSizeViaDominatorTreeAction::executeOperation(jobjectArray objects) {
    jlong heapSize = 0;
    jvmtiError err = jvmti->IterateThroughHeap(0, nullptr, nullptr, dumpHeapGraph, visitObject, &heapSize);
    if (err != JVMTI_ERROR_NONE) return nullptr;

    log("START");
    HeapDumpInfo info;
    log("Capturing heap dump...");
    progressManager.updateProgress(10, "Capturing heap dump");
    timer.start();
    err = FollowReferences(0, nullptr, nullptr, dumpHeapGraph, &info, "dump heap graph");
    timer.end();
    if (err != JVMTI_ERROR_NONE) return nullptr;

//    info.graph.replaceStartVertexAndListNeighbours(info.roots);

    log("Converting graph...");
    timer.start();
    info.graph.addNewVertexAndListNeighbours(info.roots.begin(), info.roots.end());
    for (jlong i = 1; i < info.g.size(); i++) {
        info.graph.addNewVertexAndListNeighbours(info.g[i]);
    }
    timer.end();

    log("Cleaning previous graph...");
    timer.start();
    for (jlong i = 0; i < info.g.size(); i++) {
        info.g[i].clear();
    }
    info.g.clear();
    timer.end();

    progressManager.updateProgress(10, "Calculating retained size");
    log("Calculating retained size...");
    timer.start();
    std::vector<jlong> retained_sizes = calculateRetainedSizes(info.graph, info.sizes);
    timer.end();

    log("Extracting answer");
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
