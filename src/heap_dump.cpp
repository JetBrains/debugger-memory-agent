// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#include <unordered_map>
#include <set>
#include <iostream>
#include <vector>
#include <jvmti.h>
#include <cstring>
#include <queue>
#include "gc_roots.h"
#include "types.h"
#include "utils.h"
#include "log.h"

using namespace std;

class node_data {
public:
    vector<jsize> edges;

    jlong size;

    jsize class_id = -1;
};

class data {
public:
    unordered_map<jsize, node_data> nodes;

    jsize global_index;
};


jsize get_tag_or_create(data *d, jlong *tag_ptr) {

    if (*tag_ptr == 0) {
        *tag_ptr = ++(d->global_index);
    }
    return *tag_ptr;
}

extern "C"
JNIEXPORT jint cbBuildGraph(jvmtiHeapReferenceKind referenceKind,
                            const jvmtiHeapReferenceInfo *referenceInfo, jlong classTag,
                            jlong referrerClassTag, jlong size, jlong *tagPtr,
                            jlong *referrerTagPtr, jint length, void *userData) {

    data *d = (data *)userData;

    jsize referrer_id = get_tag_or_create(d, referrerTagPtr);
    jsize referee_id = get_tag_or_create(d, tagPtr);

    if (referrerClassTag != 0) {
        node_data &referrer_data = d->nodes[referrer_id];
        referrer_data.class_id = referrerClassTag;
        referrer_data.edges.push_back(referee_id);
    }
    if (classTag != 0) {
        node_data &referee_data = d->nodes[referee_id];
        referee_data.class_id = classTag;
        referee_data.size = size;
    }

    return JVMTI_VISIT_OBJECTS;
}

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

static jobjectArray createResultObject1(JNIEnv *env, jvmtiEnv *jvmti, std::vector<jlong> &tags) {
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
    return str.find("java.") == string::npos
           && str.find("sun.") == string::npos;
}

static vector<jstring> tagClasses(JNIEnv *jni, jvmtiEnv *jvmti, jint count, jclass *classes) {
    auto result = vector<jstring>();
    if (count == 0) {
        return result;
    }

    jmethodID mid_getName = jni->GetMethodID(classes[0], "getName", "()Ljava/lang/String;");
    if (mid_getName == nullptr) {
        error("no getName!");
    }

    for (jsize i = 0; i < count; ++i) {
        jclass clazz = classes[i];
        jstring name = (jstring)jni->CallObjectMethod(clazz, mid_getName); // NOLINT(hicpp-use-auto,modernize-use-auto)
        const char *chars = jni->GetStringUTFChars(name, nullptr);
        std::string str = std::string(chars);
        jni->ReleaseStringUTFChars(name, chars);

        if (shouldSkipClass(str)) {
            continue;
        }

        result.push_back(name);
        jvmtiError err = jvmti->SetTag(clazz, i + 1);
        handleError(jvmti, err, "Could not tag class");
    }
}

jobjectArray fetchHeapDump(JNIEnv *jni, jvmtiEnv *jvmti) {
    jvmtiError err;
    jvmtiHeapCallbacks cb;

    info("Getting classes");
    jint classes_count;
    jclass *classes_ptr;
    err = jvmti->GetLoadedClasses(&classes_count, &classes_ptr);
    handleError(jvmti, err, "Could not get list of classes");
    vector<jstring> class_names = tagClasses(jni, jvmti, classes_count, classes_ptr);
    jvmti->Deallocate(reinterpret_cast<unsigned char *>(classes_ptr));

    info("Looking for paths to gc roots started");

    std::memset(&cb, 0, sizeof(jvmtiHeapCallbacks));
    cb.heap_reference_callback = reinterpret_cast<jvmtiHeapReferenceCallback>(&cbBuildGraph);

    auto graph = data();

    info("start following through references");
    err = jvmti->FollowReferences(JVMTI_HEAP_OBJECT_EITHER, nullptr, nullptr, &cb, &graph);
    handleError(jvmti, err, "FollowReference call failed");


    info("Completed. Nodes discovered:");
    info(to_string(graph.global_index).c_str());
//    info("heap tagged");
//    set<jlong> uniqueTags;
//    info("start walking through collected tags");
//    walk();
//
//    info("create resulting java objects");
//    unordered_map<jlong, jint> tag_to_index;
//    vector<jlong> tags(uniqueTags.begin(), uniqueTags.end());
    vector<jlong> v = vector<jlong>();
    jobjectArray result = createResultObject1(jni, jvmti, v);
//
//    info("remove all tags from objects in heap");
//    err = removeAllTagsFromHeap(jvmti, cleanTag);
//    handleError(jvmti, err, "Count not remove all tags");
    return result;
}
