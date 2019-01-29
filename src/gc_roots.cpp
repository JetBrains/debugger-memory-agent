// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#include <unordered_map>
#include <set>
#include <iostream>
#include <vector>
#include <jvmti.h>
#include <cstring>
#include "gc_roots.h"
#include "types.h"
#include "utils.h"

using namespace std;

using referenceInfo = std::tuple<jlong, jvmtiHeapReferenceKind, const jvmtiHeapReferenceInfo *>;
typedef struct PathNodeTag {
    std::vector<referenceInfo> backRefs;
} GcTag;

GcTag *createGcTag() {
    auto *tag = new GcTag();
    return tag;
}

jlong gcTagToPointer(GcTag *tag) {
    return (jlong) (ptrdiff_t) (void *) tag;
}

GcTag *pointerToGcTag(jlong tag_ptr) {
    if (tag_ptr == 0) {
        return new GcTag();
    }
    return (GcTag *) (ptrdiff_t) (void *) tag_ptr;
}

extern "C"
JNIEXPORT
jvmtiIterationControl cbHeapCleanupGcPaths(jlong class_tag, jlong size, jlong *tag_ptr, void *user_data) {
    if (*tag_ptr != 0) {
        GcTag *t = pointerToGcTag(*tag_ptr);
        *tag_ptr = 0;
        delete t;
    }

    return JVMTI_ITERATION_CONTINUE;
}

static void cleanHeapForGcRoots(jvmtiEnv *jvmti) {
    jvmtiError error = jvmti->IterateOverHeap(JVMTI_HEAP_OBJECT_TAGGED,
                                              reinterpret_cast<jvmtiHeapObjectCallback>(&cbHeapCleanupGcPaths),
                                              nullptr);
    handleError(jvmti, error, "Could not cleanup the heap after gc roots finding");
}

extern "C"
JNIEXPORT jint cbGcPaths(jvmtiHeapReferenceKind reference_kind,
                         const jvmtiHeapReferenceInfo *reference_info, jlong class_tag,
                         jlong referrer_class_tag, jlong size, jlong *tag_ptr,
                         jlong *referrer_tag_ptr, jint length, void *user_data) {

    if (is_ignored_reference(reference_kind)) {
        return JVMTI_VISIT_OBJECTS;
    }

    if (*tag_ptr == 0) {
        *tag_ptr = gcTagToPointer(createGcTag());
    }


    PathNodeTag *tag = pointerToGcTag(*tag_ptr);
    if (referrer_tag_ptr != nullptr) {
        if (*referrer_tag_ptr == 0) {
            *referrer_tag_ptr = gcTagToPointer(createGcTag());
        }

        tag->backRefs.emplace_back(*referrer_tag_ptr, reference_kind, reference_info);
    } else {
        // gc root found
        tag->backRefs.emplace_back(-1, reference_kind, reference_info);
    }
    return JVMTI_VISIT_OBJECTS;
}

static void walk(jlong current, std::set<jlong> &visited) {
    if (!visited.insert(current).second) return; // already visited
    for (referenceInfo &info: pointerToGcTag(current)->backRefs) {
        jlong tag = std::get<0>(info);
        if (tag != -1) {
            walk(tag, visited);
        }
    }
}

jlongArray stackInfo(JNIEnv *env, jlong threadId, jint depth, jint slot) {
    std::vector<jlong> vector = {threadId, depth, slot};
    return toJavaArray(env, vector);
}

jobjectArray methodInfo(JNIEnv *env, jvmtiEnv *jvmti, jmethodID id) {
    char *name, *signature, *genericSignature;
//    jvmti->GetMethodName(id, &name, nullptr, nullptr);
    jobjectArray result = env->NewObjectArray(3, env->FindClass("java/lang/String"), nullptr);
    env->SetObjectArrayElement(result, 0, nullptr);
    env->SetObjectArrayElement(result, 1, nullptr);
    env->SetObjectArrayElement(result, 2, nullptr);
    return result;
}

jobject convertReferenceInfo(JNIEnv *env, jvmtiEnv *jvmti, jvmtiHeapReferenceKind kind,
                             const jvmtiHeapReferenceInfo *info) {
    switch (kind) {
        case JVMTI_HEAP_REFERENCE_STATIC_FIELD:
        case JVMTI_HEAP_REFERENCE_FIELD:
            return toJavaArray(env, info->field.index);
        case JVMTI_HEAP_REFERENCE_ARRAY_ELEMENT:
            std::cout << info->array.index << std::endl;
            return toJavaArray(env, info->array.index);
        case JVMTI_HEAP_REFERENCE_CONSTANT_POOL:
            return toJavaArray(env, info->constant_pool.index);
        case JVMTI_HEAP_REFERENCE_STACK_LOCAL: {
            jvmtiHeapReferenceInfoStackLocal const &stackLocal = info->stack_local;
            return wrapWithArray(env,
                                 stackInfo(env, stackLocal.thread_id, stackLocal.depth, stackLocal.slot),
                                 methodInfo(env, jvmti, stackLocal.method)
            );
        }
        case JVMTI_HEAP_REFERENCE_JNI_LOCAL: {
            jvmtiHeapReferenceInfoJniLocal const &jniLocal = info->jni_local;
            return wrapWithArray(env,
                                 stackInfo(env, jniLocal.thread_id, jniLocal.depth, -1),
                                 methodInfo(env, jvmti, jniLocal.method));
        }
            // no information provided
        case JVMTI_HEAP_REFERENCE_CLASS:
        case JVMTI_HEAP_REFERENCE_CLASS_LOADER:
        case JVMTI_HEAP_REFERENCE_SIGNERS:
        case JVMTI_HEAP_REFERENCE_PROTECTION_DOMAIN:
        case JVMTI_HEAP_REFERENCE_INTERFACE:
        case JVMTI_HEAP_REFERENCE_SUPERCLASS:
        case JVMTI_HEAP_REFERENCE_JNI_GLOBAL:
        case JVMTI_HEAP_REFERENCE_SYSTEM_CLASS:
        case JVMTI_HEAP_REFERENCE_MONITOR:
        case JVMTI_HEAP_REFERENCE_THREAD:
        case JVMTI_HEAP_REFERENCE_OTHER:
            break;
    }

    return nullptr;
}

jintArray convertKinds(JNIEnv *env, std::vector<jvmtiHeapReferenceKind> &kinds) {
    auto kindsCount = static_cast<jsize>(kinds.size());
    jintArray result = env->NewIntArray(kindsCount);
    std::vector<jint> intKinds(kinds.begin(), kinds.end());
    env->SetIntArrayRegion(result, 0, kindsCount, intKinds.data());
    return result;
}

static jobjectArray createLinksInfos(JNIEnv *env, jvmtiEnv *jvmti, jlong tag, unordered_map<jlong, jint> tagToIndex) {
    GcTag *gcTag = pointerToGcTag(tag);
    std::vector<jint> prevIndices;
    std::vector<jint> refKinds;
    std::vector<jobject> refInfos;

    for (referenceInfo &info : gcTag->backRefs) {
        jlong prevTag = std::get<0>(info);
        prevIndices.push_back(prevTag == -1 ? -1 : tagToIndex[prevTag]);
        auto kind = std::get<1>(info);
        refKinds.push_back(static_cast<jint>(kind));
        refInfos.push_back(convertReferenceInfo(env, jvmti, kind, std::get<2>(info)));
    }

    jobjectArray result = env->NewObjectArray(3, env->FindClass("java/lang/Object"), nullptr);
    env->SetObjectArrayElement(result, 0, toJavaArray(env, prevIndices));
    env->SetObjectArrayElement(result, 1, toJavaArray(env, refKinds));
    env->SetObjectArrayElement(result, 2, toJavaArray(env, refInfos));

    return result;
}

static jobjectArray createResultObject(JNIEnv *env, jvmtiEnv *jvmti, std::vector<jlong> &tags) {
    jclass langObject = env->FindClass("java/lang/Object");
    jobjectArray result = env->NewObjectArray(2, langObject, nullptr);
    jint objectsCount = 0;
    jobject *objects;
    jlong *objTags;
    jvmtiError err;

    err = jvmti->GetObjectsWithTags(static_cast<jint>(tags.size()), tags.data(), &objectsCount, &objects, &objTags);
    handleError(jvmti, err, "Could not receive objects by their tags");
    // TODO: Assert objectsCount == tags.size()

    jobjectArray resultObjects = env->NewObjectArray(objectsCount, langObject, nullptr);
    jobjectArray links = env->NewObjectArray(objectsCount, langObject, nullptr);

    unordered_map<jlong, jint> tagToIndex;
    for (jsize i = 0; i < objectsCount; ++i) {
        tagToIndex[objTags[i]] = i;
    }

    for (jsize i = 0; i < objectsCount; ++i) {
        env->SetObjectArrayElement(resultObjects, i, objects[i]);
        auto infos = createLinksInfos(env, jvmti, objTags[i], tagToIndex);
        env->SetObjectArrayElement(links, i, infos);
    }

    env->SetObjectArrayElement(result, 0, resultObjects);
    env->SetObjectArrayElement(result, 1, links);

    return result;
}

jobjectArray findGcRoots(JNIEnv *jni, jvmtiEnv *jvmti, jclass thisClass, jobject object) {
    jvmtiError err;
    jvmtiHeapCallbacks cb;
    std::memset(&cb, 0, sizeof(jvmtiHeapCallbacks));
    cb.heap_reference_callback = reinterpret_cast<jvmtiHeapReferenceCallback>(&cbGcPaths);

    GcTag *tag = createGcTag();
    err = jvmti->SetTag(object, gcTagToPointer(tag));
    handleError(jvmti, err, "Could not set tag for target object");
    err = jvmti->FollowReferences(JVMTI_HEAP_OBJECT_EITHER, nullptr, nullptr, &cb, nullptr);
    handleError(jvmti, err, "FollowReference call failed");

    set<jlong> unique_tags;
    walk(gcTagToPointer(tag), unique_tags);

    unordered_map<jlong, jint> tag_to_index;
    vector<jlong> tags(unique_tags.begin(), unique_tags.end());
    jobjectArray result = createResultObject(jni, jvmti, tags);

    cleanHeapForGcRoots(jvmti);
    return result;
}