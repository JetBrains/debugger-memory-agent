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

typedef struct PathNodeTag {
    std::vector<jlong> prev;
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
    jvmtiError error = jvmti->IterateOverHeap(JVMTI_HEAP_OBJECT_TAGGED, reinterpret_cast<jvmtiHeapObjectCallback>(&cbHeapCleanupGcPaths), nullptr);
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

    if (referrer_tag_ptr != nullptr) {
        if (*referrer_tag_ptr == 0) {
            *referrer_tag_ptr = gcTagToPointer(createGcTag());
        }

        PathNodeTag *tag = pointerToGcTag(*tag_ptr);
        tag->prev.push_back(*referrer_tag_ptr);
    }
    return JVMTI_VISIT_OBJECTS;
}

static void walk(jlong current, std::set<jlong> &visited) {
    visited.insert(current);
    if (visited.find(current) != visited.end()) {
        for (jlong tag : pointerToGcTag(current)->prev) {
            walk(tag, visited);
        }
    }
}

static jobjectArray createJavaArrayWithObjectsByTags(JNIEnv *env,
                                                     jvmtiEnv *jvmti,
                                                     std::vector<jlong> &tags,
                                                     std::unordered_map<jlong, jint> &tag_to_index) {
    jint count;
    jobject *objects;
    jlong *result_tags;
    jvmtiError err;
    // TODO: add error handling
    // Note: order of objects in the result of GetObjectsWithTags may differ from tags
    err = jvmti->GetObjectsWithTags(static_cast<jint >(tags.size()), tags.data(), &count, &objects, &result_tags);
    handleError(jvmti, err, "Could not receive objects by their tags");
    if (count != tags.size()) {
        std::cerr << "could not find all objects by their tags. Found: " << count << ". Expected: " << tags.size()
                  << std::endl;
        return nullptr;
    }
    // TODO: Reallocate memory
    std::vector<jobject> object_in_correct_order(static_cast<const uint64_t>(count));
    for (int i = 0; i < count; i++) {
        jint object_order = tag_to_index[result_tags[i]];
        object_in_correct_order[object_order] = objects[i];
    }

    return toJavaArray(env, object_in_correct_order.data(), count);
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
    cout << "Total reachable unique tags: " << unique_tags.size() << endl;

    unordered_map<jlong, jint> tag_to_index;
    vector<jlong> tags;
    jint i = 0;
    for (jlong reachable_tag: unique_tags) {
        tags.push_back(reachable_tag);
        tag_to_index[reachable_tag] = i;
        ++i;
    }

    cout << "start building reversed links" << endl;
    vector<vector<jint>> previous_links(unique_tags.size());
    for (jlong reachable_tag: unique_tags) {
        jint index = tag_to_index[reachable_tag];
        for (jlong prev_tag: pointerToGcTag(reachable_tag)->prev) {
            jint previous_index = tag_to_index[prev_tag];
            previous_links[index].push_back(previous_index);
        }
    }

    cout << "prepare result arrays" << endl;

    jobjectArray objects = createJavaArrayWithObjectsByTags(jni, jvmti, tags, tag_to_index);
    jobjectArray prev = toJavaArray(jni, previous_links);
    jobjectArray result = wrapWithArray(jni, objects, prev);

    cleanHeapForGcRoots(jvmti);
    return result;
}