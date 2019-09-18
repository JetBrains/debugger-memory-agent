// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#include <unordered_map>
#include <iostream>
#include <vector>
#include <jvmti.h>
#include <cstring>
#include <algorithm>
#include "utils.h"
#include "log.h"

using namespace std;

FILE *fp;

const jlong SHIFT_SIZE = 30;
const jlong SHIFT = 1 << SHIFT_SIZE;
const jlong NODE_MASK = SHIFT - 1;


class node_data {
public:
    node_data() : edges(), size(0), class_id(0) {}

    ~node_data() {
        edges.clear();
    }

    vector<jsize> edges;

    jlong size;

    jsize class_id;
};

class data {
public:
    data() : global_index(0), nodes(unordered_map<jsize, node_data>()) {}

    ~data() {
        nodes.clear();
    }

    unordered_map<jsize, node_data> nodes;

    jsize global_index;
};

#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
void outputGraph(unordered_map<jsize, node_data> &graph) {
    for (auto &p : graph) {
        fprintf(fp, "Edges for node %d\n", p.first);
        for (auto v : p.second.edges) {
            fprintf(fp, "%d ", v);
        }
        fprintf(fp, "\n");
    }
    fflush(fp);
}
#pragma clang diagnostic pop


#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
void cleanTag1(jlong tag) {
#pragma clang diagnostic pop
}

jsize get_tag_or_create(data *d, jlong *tag_ptr) {
    if (tag_ptr == nullptr) {
        return 0;
    }
    if ((*tag_ptr & NODE_MASK) == 0) {
        *tag_ptr += ++(d->global_index);
    }

    return (jsize)(*tag_ptr & NODE_MASK);
}

extern "C"
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
JNIEXPORT jint cbBuildGraph(jvmtiHeapReferenceKind referenceKind,
                            const jvmtiHeapReferenceInfo *referenceInfo, jlong classTag,
                            jlong referrerClassTag, jlong size, jlong *tagPtr,
                            jlong *referrerTagPtr, jint length, void *userData) {

    data *d = (data *)userData;

    jsize referrer_id = get_tag_or_create(d, referrerTagPtr);
    jsize referee_id = get_tag_or_create(d, tagPtr);
    if (referee_id == 0) return JVMTI_VISIT_OBJECTS;

    if (referrer_id != 0) {
        node_data &referrer_data = d->nodes[referrer_id];
        referrer_data.edges.push_back(referee_id);
        if (referrerClassTag >= SHIFT) {
            referrer_data.class_id = referrerClassTag >> SHIFT_SIZE;
        }
    }

    node_data &referee_data = d->nodes[referee_id];
    referee_data.size = size;
    if (classTag >= SHIFT) {
        referee_data.class_id = classTag >> SHIFT_SIZE;
    }

    return JVMTI_VISIT_OBJECTS;
}
#pragma clang diagnostic pop

/*
static void walk(jlong start, set<jlong> &visited, jint limit) {
    std::queue<jlong> queue;
    queue.push(start);
    jlong tag;
    while (!queue.empty() && visited.size() <= limit) {
        tag = queue.front();
        queue.pop();
        visited.insert(tag);
        for (referenceInfo *info: pointerToGcTag(tag)->backRefs) {
            jlong parentTag = info->tag();
            if (parentTag != -1 && visited.find(parentTag) == visited.end()) {
                queue.push(parentTag);
            }
        }
    }
}
*/


/*
static jobjectArray createLinksInfos(JNIEnv *env, jvmtiEnv *jvmti, jlong tag, unordered_map<jlong, jint> tagToIndex) {
    GcTag *gcTag = pointerToGcTag(tag);
    std::vector<jint> prevIndices;
    std::vector<jint> refKinds;
    std::vector<jobject> refInfos;

    jint exceedLimitCount = 0;
    for (referenceInfo *info : gcTag->backRefs) {
        jlong prevTag = info->tag();
        if (prevTag != -1 && tagToIndex.find(prevTag) == tagToIndex.end()) {
            ++exceedLimitCount;
            continue;
        }
        prevIndices.push_back(prevTag == -1 ? -1 : tagToIndex[prevTag]);
        refKinds.push_back(static_cast<jint>(info->kind()));
        refInfos.push_back(info->getReferenceInfo(env, jvmti));
    }


    jobjectArray result = env->NewObjectArray(4, env->FindClass("java/lang/Object"), nullptr);
    env->SetObjectArrayElement(result, 0, toJavaArray(env, prevIndices));
    env->SetObjectArrayElement(result, 1, toJavaArray(env, refKinds));
    env->SetObjectArrayElement(result, 2, toJavaArray(env, refInfos));
    env->SetObjectArrayElement(result, 3, toJavaArray(env, exceedLimitCount));
    return result;
}
*/

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
static jobjectArray createResultObject1(JNIEnv *env, jvmtiEnv *jvmti, std::vector<jlong> &tags) {
#pragma clang diagnostic pop
    jclass langObject = env->FindClass("java/lang/Object");
    jobjectArray result = env->NewObjectArray(2, langObject, nullptr);
    return result;
//    jvmtiError err;
//
//    std::vector<std::pair<jobject, jlong>> objectToTag;
//    err = cleanHeapAndGetObjectsByTags(jvmti, tags, objectToTag, cleanTag);
//    handleError(jvmti, err, "Could not receive objects by their tags");
//    // TODO: Assert objectsCount == tags.size()
//    jint objectsCount = static_cast<jint>(objectToTag.size());
//
//    jobjectArray resultObjects = env->NewObjectArray(objectsCount, langObject, nullptr);
//    jobjectArray links = env->NewObjectArray(objectsCount, langObject, nullptr);
//
//    unordered_map<jlong, jint> tagToIndex;
//    for (jsize i = 0; i < objectsCount; ++i) {
//        tagToIndex[objectToTag[i].second] = i;
//    }
//
//    for (jsize i = 0; i < objectsCount; ++i) {
//        env->SetObjectArrayElement(resultObjects, i, objectToTag[i].first);
//        auto infos = createLinksInfos(env, jvmti, objectToTag[i].second, tagToIndex);
//        env->SetObjectArrayElement(links, i, infos);
//    }
//
//    env->SetObjectArrayElement(result, 0, resultObjects);
//    env->SetObjectArrayElement(result, 1, links);
//
//    return result;
}

static bool shouldSkipClass(const string &str) {
    return str.find("Ljava/") != string::npos
           || str.find("Lsun/") != string::npos
           || str.size() < 5;
}

static vector<string> tagClasses(jvmtiEnv *jvmti, jint count, jclass *classes) {
    auto result = vector<string>();
    if (count == 0) {
        return result;
    }

    char *signature;
    for (jsize i = 0; i < count; ++i) {
        jclass clazz = classes[i];
        jvmtiError err = jvmti->GetClassSignature(clazz, &signature, nullptr);
        handleError(jvmti, err, "failed to get signature");
        std::string str = std::string(signature);
        jvmti->Deallocate(reinterpret_cast<unsigned char *>(signature));

        if (shouldSkipClass(str)) {
            continue;
        }

        result.push_back(str);
        err = jvmti->SetTag(clazz, result.size() << SHIFT_SIZE);
        handleError(jvmti, err, "Could not tag class");
    }
    return result;
}

static void untagClasses(jvmtiEnv *jvmti, jint count, jclass *classes) {
    if (count == 0) {
        return;
    }

    for (jsize i = 0; i < count; ++i) {
        jclass clazz = classes[i];
        jvmtiError err = jvmti->SetTag(clazz, 0);
        handleError(jvmti, err, "Could not tag class");
    }
}

static void dfs1(jsize v,
                 unordered_map<jsize, node_data> &graph,
                 vector<jsize> &order,
                 vector<bool> &visited) {
    visited[v] = true;

    for (auto i : graph[v].edges) {
        if (!visited[i]) {
            dfs1(i, graph, order, visited);
        }
    }
    order.push_back(v);
}

static unordered_map<jsize, node_data> transpose(unordered_map<jsize, node_data> &graph) {
    unordered_map<jsize, node_data> result = unordered_map<jsize, node_data>();

    for (auto &pair : graph) {
        for (auto v : pair.second.edges) {
            result[v].edges.push_back(pair.first);
        }
    }

    return result;
}

static vector<vector<jsize>> condense(data &data) {
    jsize nodes_count = data.global_index;
    vector<jsize> order = vector<jsize>();
    vector<bool> visited = vector<bool>(nodes_count + 1);

    for (int i = 1; i <= nodes_count; ++i) {
        if (!visited[i]) {
            dfs1(i, data.nodes, order, visited);
        }
    }

    unordered_map<jsize, node_data> transposed = transpose(data.nodes);
//    outputGraph(transposed);

    reverse(begin(order), end(order));

    vector<vector<jsize>> components = vector<vector<jsize>>();
    visited.assign(nodes_count + 1, false);
    for (auto v : order) {
        if (!visited[v]) {
            vector<jsize> current_component;
            dfs1(v, transposed, current_component, visited);
            components.push_back(current_component);
        }
    }

    info("Condensation finished. Components count:");
    info(to_string(components.size()).c_str());
    return components;
}

jobjectArray fetchHeapDump(JNIEnv *jni, jvmtiEnv *jvmti) {
    fp=fopen("/Users/valich/work/debugger-memory-agent/log.txt", "w");
    jvmtiError err;
    jvmtiHeapCallbacks cb;

    info("Getting classes");
    jint classes_count;
    jclass *classes_ptr;
    err = jvmti->GetLoadedClasses(&classes_count, &classes_ptr);
    handleError(jvmti, err, "Could not get list of classes");
    vector<string> class_names = tagClasses(jvmti, classes_count, classes_ptr);

    info("Looking for paths to gc roots started");

    std::memset(&cb, 0, sizeof(jvmtiHeapCallbacks));
    cb.heap_reference_callback = reinterpret_cast<jvmtiHeapReferenceCallback>(&cbBuildGraph);

    auto graph = data();

    info("start following through references");
    err = jvmti->FollowReferences(JVMTI_HEAP_OBJECT_EITHER, nullptr, nullptr, &cb, &graph);
    handleError(jvmti, err, "FollowReference call failed");

    untagClasses(jvmti, classes_count, classes_ptr);
    jvmti->Deallocate(reinterpret_cast<unsigned char *>(classes_ptr));

    info("Completed. Nodes discovered:");
    info(to_string(graph.global_index).c_str());

//    outputGraph(graph.nodes);

    info("start shortening the graph");
    const vector<vector<jsize>> &components = condense(graph);

    fprintf(fp, "Component %d\n", (int)components.size());
    for (const auto &comp : components) {
        if (comp.size() < 2) continue;
        for (auto v : comp) {
            jsize class_id = graph.nodes[v].class_id;
            fprintf(fp, "%s ", class_id == 0 ? "0" : class_names[class_id - 1].c_str());
        }
        fprintf(fp, "\n");
        fflush(fp);
    }
//    walk();
//
//    info("create resulting java objects");
//    unordered_map<jlong, jint> tag_to_index;
//    vector<jlong> tags(uniqueTags.begin(), uniqueTags.end());
    vector<jlong> v = vector<jlong>();
    jobjectArray result = createResultObject1(jni, jvmti, v);
    err = removeAllTagsFromHeap(jvmti, cleanTag1);
    handleError(jvmti, err, "Count not remove all tags");
//
//    info("remove all tags from objects in heap");
    return result;
}