// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#include <jvmti.h>
#include <vector>
#include <iostream>
#include <cstring>
#include "object_size.h"
#include "utils.h"
#include "types.h"

using namespace std;

static int tag_balance_for_sizes = 0;

static Tag *pointerToTag(jlong tag_ptr) {
    if (tag_ptr == 0) {
        std::cerr << "unexpected zero-tag value" << std::endl;
        return nullptr;
    }
    return (Tag *) (ptrdiff_t) (void *) tag_ptr;
}

static jlong tagToPointer(Tag *tag) {
    return (jlong) (ptrdiff_t) (void *) tag;
}

static Tag *createTag(bool start, bool in_subtree, bool reachable_outside) {
    auto *tag = new Tag();
    ++tag_balance_for_sizes;
    tag->in_subtree = in_subtree;
    tag->start_object = start;
    tag->reachable_outside = reachable_outside;
    return tag;
}

extern "C"
JNIEXPORT
jvmtiIterationControl cbHeapCleanupSizeTags(jlong class_tag, jlong size, jlong *tag_ptr, void *user_data) {
    if (*tag_ptr != 0) {
        Tag *t = pointerToTag(*tag_ptr);
        delete t;
        *tag_ptr = 0;
        --tag_balance_for_sizes;
    }

    return JVMTI_ITERATION_CONTINUE;
}

static void cleanHeapForSizes(jvmtiEnv *jvmti) {
    // For some reason this call does not iterate through all objects :( Please, do not use it
//    gdata->jvmti->IterateOverReachableObjects(NULL, NULL, &cbHeapCleanupSizeTags, NULL);
    jvmtiError err = jvmti->IterateOverHeap(JVMTI_HEAP_OBJECT_TAGGED, reinterpret_cast<jvmtiHeapObjectCallback>(&cbHeapCleanupSizeTags), nullptr);
    handleError(jvmti, err, "Could cleanup heap after size estimating");
}

extern "C"
JNIEXPORT jint cbHeapReference(jvmtiHeapReferenceKind reference_kind,
                               const jvmtiHeapReferenceInfo *reference_info, jlong class_tag,
                               jlong referrer_class_tag, jlong size, jlong *tag_ptr,
                               jlong *referrer_tag_ptr, jint length, void *user_data) {
    if (is_ignored_reference(reference_kind)) {
        return JVMTI_VISIT_OBJECTS;
    }

    if (referrer_tag_ptr == nullptr) {
        if (tag_ptr == nullptr) {
            cerr << "Unexpected null pointer to referrer and referee tags. Reference kind: "
                 << get_reference_type_description(reference_kind) << endl;
            return JVMTI_VISIT_ABORT;
        }

        if (*tag_ptr != 0) {
            Tag *tag = pointerToTag(*tag_ptr);
            if (!tag->start_object) {
                tag->reachable_outside = true;
            }
        } else {
            *tag_ptr = tagToPointer(createTag(false, false, true));
        }
        return JVMTI_VISIT_OBJECTS;
    }

    if (*referrer_tag_ptr != 0) {
        //referrer has tag
        Tag *referrer_tag = pointerToTag(*referrer_tag_ptr);
        if (referrer_tag->in_subtree) {
            cout << get_tag_description(referrer_tag) << "" << get_reference_type_description(reference_kind)
                 << " link from initial object subtree" << endl;
            cout << *tag_ptr << endl;
        }
        if (*tag_ptr != 0) {
            Tag *referee_tag = pointerToTag(*tag_ptr);
            if (referrer_tag->in_subtree) {
                referee_tag->in_subtree = true;
                ((vector<jlong> *) (ptrdiff_t) user_data)->push_back(*tag_ptr);
            }
            if (referrer_tag->reachable_outside && !referee_tag->start_object) {
                referee_tag->reachable_outside = true;
            }
        } else {
            Tag *referee_tag = createTag(false, referrer_tag->in_subtree, referrer_tag->reachable_outside);
            *tag_ptr = tagToPointer(referee_tag);
            if (referee_tag->in_subtree) ((vector<jlong> *) (ptrdiff_t) user_data)->push_back(*tag_ptr);
        }
    } else {
        // referrer has no tag yet
        Tag *referrer_tag = createTag(false, false, true);
        if (*tag_ptr != 0) {
            Tag *referee_tag = pointerToTag(*tag_ptr);
            if (!referee_tag->start_object) {
                referee_tag->reachable_outside = true;
            }
        } else {
            Tag *referee_tag = createTag(false, false, true);
            *tag_ptr = tagToPointer(referee_tag);
        }

        *referrer_tag_ptr = tagToPointer(referrer_tag);
    }

    return JVMTI_VISIT_OBJECTS;
}

jint estimateObjectSize(JNIEnv *jni, jvmtiEnv *jvmti, jclass thisClass, jobject object) {
    jvmtiError err;
    vector<jlong> tags;
    jvmtiHeapCallbacks cb;
    std::memset(&cb, 0, sizeof(jvmtiHeapCallbacks));
    cb.heap_reference_callback = reinterpret_cast<jvmtiHeapReferenceCallback>(&cbHeapReference);

    Tag *tag = createTag(true, true, false);
    tags.push_back(tagToPointer(tag));
    err = jvmti->SetTag(object, tagToPointer(tag));
    handleError(jvmti, err, "Could not set a tag for target object");
    jint count = 0;
    err = jvmti->FollowReferences(JVMTI_HEAP_OBJECT_EITHER, nullptr, nullptr, &cb, &tags);
    handleError(jvmti, err, "Could not follow references for object size estimation");
    jobject *objects;
    jlong *objects_tags;
    err = jvmti->GetObjectsWithTags(static_cast<jint>(tags.size()), tags.data(), &count, &objects, &objects_tags);
    handleError(jvmti, err, "Could not get objects by their tags");

    jlong retainedSize = 0;
    jint objectsInSubtree = 0;
    jint retainedObjectsCount = 0;
    for (int i = 0; i < count; i++) {
        jlong size;
        jobject obj = objects[i];
        objectsInSubtree++;
        if (!pointerToTag(objects_tags[i])->reachable_outside) {
            jvmti->GetObjectSize(obj, &size);
            retainedObjectsCount++;
            retainedSize += size;
        }
    }

    cout << "Total objects in the subtree: " << objectsInSubtree << endl;
    cout << "Total objects retained: " << retainedObjectsCount << endl;
    cout << "Retained size: " << retainedSize << endl;

    cleanHeapForSizes(jvmti);

    if (tag_balance_for_sizes != 0) {
        cerr << "tag balance is not zero: " << tag_balance_for_sizes << endl;
    }
    return static_cast<jint>(retainedSize);
}
