// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#include <jvmti.h>
#include <vector>
#include <set>
#include <iostream>
#include <cstring>
#include "object_size.h"
#include "utils.h"
#include "types.h"
#include "log.h"

using namespace std;

static Tag *pointerToTag(jlong tagPtr) {
    if (tagPtr == 0) {
        std::cerr << "unexpected zero-tag value" << std::endl;
        return nullptr;
    }
    return (Tag *) (ptrdiff_t) (void *) tagPtr;
}

static jlong tagToPointer(Tag *tag) {
    return (jlong) (ptrdiff_t) (void *) tag;
}

static Tag *createTag(bool start, bool inSubtree, bool reachableOutside) {
    auto *tag = new Tag();
    tag->inSubtree = inSubtree;
    tag->startObject = start;
    tag->reachableOutside = reachableOutside;
    return tag;
}

extern "C"
JNIEXPORT
jvmtiIterationControl cbHeapCleanupSizeTags(jlong classTag, jlong size, jlong *tagPtr, void *userData) {
    if (*tagPtr != 0) {
        Tag *t = pointerToTag(*tagPtr);
        delete t;
        *tagPtr = 0;
    }

    return JVMTI_ITERATION_CONTINUE;
}

static void cleanHeapForSizes(jvmtiEnv *jvmti) {
    jvmtiError err = jvmti->IterateOverHeap(JVMTI_HEAP_OBJECT_TAGGED,
                                            reinterpret_cast<jvmtiHeapObjectCallback>(&cbHeapCleanupSizeTags), nullptr);
    handleError(jvmti, err, "Could cleanup heap after size estimating");
}

extern "C"
JNIEXPORT jint cbHeapReference(jvmtiHeapReferenceKind referenceKind,
                               const jvmtiHeapReferenceInfo *referenceInfo, jlong classTag,
                               jlong referrerClassTag, jlong size, jlong *tagPtr,
                               jlong *referrerTagPtr, jint length, void *userData) {
    if (referrerTagPtr == nullptr) {
        if (tagPtr == nullptr) {
            cerr << "Unexpected null pointer to referrer and referee tags. Reference kind: "
                 << getReferenceTypeDescription(referenceKind) << endl;
            return JVMTI_VISIT_ABORT;
        }

        if (*tagPtr != 0) {
            Tag *tag = pointerToTag(*tagPtr);
            if (!tag->startObject) {
                tag->reachableOutside = true;
            }
        } else {
            *tagPtr = tagToPointer(createTag(false, false, true));
        }
        return JVMTI_VISIT_OBJECTS;
    }

    if (*referrerTagPtr != 0) {
        //referrer has tag
        Tag *referrerTag = pointerToTag(*referrerTagPtr);
        if (*tagPtr != 0) {
            Tag *refereeTag = pointerToTag(*tagPtr);
            if (referrerTag->inSubtree) {
                refereeTag->inSubtree = true;
                ((set<jlong> *) (ptrdiff_t) userData)->insert(*tagPtr);
            }
            if (referrerTag->reachableOutside && !refereeTag->startObject) {
                refereeTag->reachableOutside = true;
            }
        } else {
            Tag *refereeTag = createTag(false, referrerTag->inSubtree, referrerTag->reachableOutside);
            *tagPtr = tagToPointer(refereeTag);
            if (refereeTag->inSubtree) ((set<jlong> *) (ptrdiff_t) userData)->insert(*tagPtr);
        }
    } else {
        // referrer has no tag yet
        Tag *referrerTag = createTag(false, false, true);
        if (*tagPtr != 0) {
            Tag *refereeTag = pointerToTag(*tagPtr);
            if (!refereeTag->startObject) {
                refereeTag->reachableOutside = true;
            }
        } else {
            Tag *refereeTag = createTag(false, false, true);
            *tagPtr = tagToPointer(refereeTag);
        }

        *referrerTagPtr = tagToPointer(referrerTag);
    }

    return JVMTI_VISIT_OBJECTS;
}

jlong estimateObjectSize(JNIEnv *jni, jvmtiEnv *jvmti, jclass thisClass, jobject object) {
    jvmtiError err;
    std::set<jlong> tags;
    jvmtiHeapCallbacks cb;
    info("Looking for retained set started");

    std::memset(&cb, 0, sizeof(jvmtiHeapCallbacks));
    cb.heap_reference_callback = reinterpret_cast<jvmtiHeapReferenceCallback>(&cbHeapReference);

    Tag *tag = createTag(true, true, false);
    tags.insert(tagToPointer(tag));
    err = jvmti->SetTag(object, tagToPointer(tag));
    handleError(jvmti, err, "Could not set a tag for target object");
    jint count = 0;

    info("start following through references");
    err = jvmti->FollowReferences(JVMTI_HEAP_OBJECT_EITHER, nullptr, nullptr, &cb, &tags);
    handleError(jvmti, err, "Could not follow references for object size estimation");
    info("heap tagged");
    jobject *objects;
    jlong *objectsTags;
    info("create resulting java objects");
    vector<jlong> retainedTags(tags.begin(), tags.end());
    err = jvmti->GetObjectsWithTags(static_cast<jint>(tags.size()),
                                    retainedTags.data(), &count, &objects,
                                    &objectsTags);
    handleError(jvmti, err, "Could not get objects by their tags");

    jlong retainedSize = 0;
    jint objectsInSubtree = 0;
    jint retainedObjectsCount = 0;
    for (int i = 0; i < count; i++) {
        jlong size;
        jobject obj = objects[i];
        objectsInSubtree++;
        if (!pointerToTag(objectsTags[i])->reachableOutside) {
            jvmti->GetObjectSize(obj, &size);
            retainedObjectsCount++;
            retainedSize += size;
        }
    }

    info("remove all tags from objects in heap");
    cleanHeapForSizes(jvmti);

    return retainedSize;
}
