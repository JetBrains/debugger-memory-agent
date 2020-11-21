// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#include <vector>
#include "retained_size_and_held_objects.h"
#include "retained_size_action.h"
#include "sizes_tags.h"

jint JNICALL countSizeAndRetagHeldObjects(jlong classTag, jlong size, jlong *tagPtr, jint length, void *userData) {
    if (*tagPtr == 0) {
        return JVMTI_ITERATION_CONTINUE;
    }

    Tag *tag = tagToPointer(*tagPtr);
    *tagPtr = 0;
    for (query_size_t i = 0; i < tag->array.getSize(); i++) {
        const TagInfoArray::TagInfo &info = tag->array[i];
        if (isRetained(info.state)) {
            *tagPtr = pointerToTag(&Tag::HeldObjectTag);
            *reinterpret_cast<jlong *>(userData) += size;
        }
    }

    tag->unref();
    return JVMTI_ITERATION_CONTINUE;
}

jint JNICALL walkHeapAndSkipStartObject(jvmtiHeapReferenceKind refKind, const jvmtiHeapReferenceInfo *refInfo, jlong classTag,
                                        jlong referrerClassTag, jlong size, jlong *tagPtr,
                                        jlong *referrerTagPtr, jint length, void *userData) {
    if (*tagPtr != 0 && tagToPointer(*tagPtr)->isStartTag) {
        return 0;
    }

    return visitReference(refKind, refInfo, classTag, referrerClassTag, size, tagPtr, referrerTagPtr, length, userData);
}

RetainedSizeAndHeldObjectsAction::RetainedSizeAndHeldObjectsAction(JNIEnv *env, jvmtiEnv *jvmti, jobject cancellationFileName, jlong duration) : MemoryAgentTimedAction(env, jvmti, cancellationFileName, duration) {

}

jvmtiError RetainedSizeAndHeldObjectsAction::retagStartObject(jobject object) {
    jlong oldTag;
    jvmtiError err = jvmti->GetTag(object, &oldTag);
    if (!isOk(err)) return err;

    if (isTagWithNewInfo(oldTag)) {
        Tag *tag = Tag::create(0, createState(true, true, false, false));
        err = jvmti->SetTag(object, pointerToTag(tag));
    }

    return err;
}

jvmtiError RetainedSizeAndHeldObjectsAction::estimateObjectSize(jobject &object, jlong &retainedSize, std::vector<jobject> &heldObjects) {
    Tag *tag = Tag::create(0, createState(true, true, false, false));
    jvmtiError err = jvmti->SetTag(object, pointerToTag(tag));
    if (!isOk(err)) return err;
    if (shouldStopExecution()) return MEMORY_AGENT_INTERRUPTED_ERROR;

    err = FollowReferences(0, nullptr, nullptr, walkHeapAndSkipStartObject, nullptr, "tag heap");
    if (!isOk(err)) return err;
    if (shouldStopExecution()) return MEMORY_AGENT_INTERRUPTED_ERROR;

    err = FollowReferences(0, nullptr, object, visitReference, nullptr, "tag heap");
    if (!isOk(err)) return err;
    if (shouldStopExecution()) return MEMORY_AGENT_INTERRUPTED_ERROR;

    retainedSize = 0;
    err = IterateThroughHeap(JVMTI_HEAP_FILTER_UNTAGGED, nullptr, countSizeAndRetagHeldObjects,
                             &retainedSize, "calculate retained size");
    if (!isOk(err)) return err;
    if (shouldStopExecution()) return MEMORY_AGENT_INTERRUPTED_ERROR;

    if (sizesTagBalance != 0) {
        fatal("MEMORY LEAK FOUND!");
    }

    debug("collect held objects");
    return getObjectsByTags(jvmti, std::vector<jlong>{pointerToTag(&Tag::HeldObjectTag)}, heldObjects);
}

jobjectArray RetainedSizeAndHeldObjectsAction::createResultObject(jlong retainedSize, jlong shallowSize, const std::vector<jobject> &heldObjects) {
    jint objectsCount = static_cast<jint>(heldObjects.size());
    jclass langObject = env->FindClass("java/lang/Object");
    jobjectArray resultObjects = env->NewObjectArray(objectsCount, langObject, nullptr);

    for (jsize i = 0; i < objectsCount; ++i) {
        env->SetObjectArrayElement(resultObjects, i, heldObjects[i]);
    }

    jobjectArray result = env->NewObjectArray(2, langObject, nullptr);
    std::vector<jlong> sizes{shallowSize, retainedSize};
    env->SetObjectArrayElement(result, 0, toJavaArray(env, sizes));
    env->SetObjectArrayElement(result, 1, resultObjects);

    return result;
}

jobjectArray RetainedSizeAndHeldObjectsAction::executeOperation(jobject object) {
    std::vector<jobject> objects;
    objects.push_back(object);
    std::vector<jobject> heldObjects;
    jlong retainedSize;
    jlong shallowSize;
    jvmtiError err = estimateObjectSize(object, retainedSize, heldObjects);
    if (!isOk(err)) {
        handleError(jvmti, err, "Could not estimate object size");
    }

    err = jvmti->GetObjectSize(object, &shallowSize);
    if (!isOk(err)) {
        handleError(jvmti, err, "Could not estimate object's shallow size");
    }

    return createResultObject(retainedSize, shallowSize, heldObjects);
}

jvmtiError RetainedSizeAndHeldObjectsAction::cleanHeap() {
    jvmtiHeapCallbacks cb;
    std::memset(&cb, 0, sizeof(jvmtiHeapCallbacks));
    cb.heap_iteration_callback = clearTag;
    jvmtiError err =  this->jvmti->IterateThroughHeap(0, nullptr, &cb, nullptr);

    if (sizesTagBalance != 0) {
        fatal("MEMORY LEAK FOUND!");
    }

    return err;
}
