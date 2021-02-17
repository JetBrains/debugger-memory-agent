// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#include <vector>
#include "retained_size_and_held_objects.h"
#include "sizes_tags.h"

#define START_TAG 1
#define HELD_OBJECT_TAG 2
#define VISITED_TAG 3

jint JNICALL firstTraversal(jvmtiHeapReferenceKind refKind, const jvmtiHeapReferenceInfo *refInfo, jlong classTag,
                            jlong referrerClassTag, jlong size, jlong *tagPtr,
                            jlong *referrerTagPtr, jint length, void *userData) {
    if (refKind == JVMTI_HEAP_REFERENCE_JNI_LOCAL || refKind == JVMTI_HEAP_REFERENCE_JNI_GLOBAL) {
        return 0;
    } else if (*tagPtr == START_TAG) {
        return 0;
    } else if (*tagPtr == 0) {
        *tagPtr = VISITED_TAG;
    }

    return JVMTI_VISIT_OBJECTS;
}

jint JNICALL secondTraversal(jvmtiHeapReferenceKind refKind, const jvmtiHeapReferenceInfo *refInfo, jlong classTag,
                            jlong referrerClassTag, jlong size, jlong *tagPtr,
                            jlong *referrerTagPtr, jint length, void *userData) {
    if (*tagPtr == 0) {
        *reinterpret_cast<jlong *>(userData) += size;
        *tagPtr = HELD_OBJECT_TAG;
    }

    return JVMTI_VISIT_OBJECTS;
}

RetainedSizeAndHeldObjectsAction::RetainedSizeAndHeldObjectsAction(JNIEnv *env, jvmtiEnv *jvmti, jobject object) : MemoryAgentAction(env, jvmti, object) {

}

jvmtiError RetainedSizeAndHeldObjectsAction::traverseHeapForTheFirstTime(jobject &object) {
    jvmtiError err = jvmti->SetTag(object, START_TAG);
    if (!isOk(err)) return err;
    if (shouldStopExecution()) return MEMORY_AGENT_INTERRUPTED_ERROR;

    updateProgress(10, "Traversing heap for the first time...");
    return FollowReferences(0, nullptr, nullptr, firstTraversal, nullptr, "tag heap");
}

jvmtiError RetainedSizeAndHeldObjectsAction::traverseHeapFromStartObjectAndCountRetainedSize(jobject &object, jlong &retainedSize) {
    updateProgress(45, "Traversing heap for the second time...");
    retainedSize = 0;
    jvmtiError err = FollowReferences(0, nullptr, object, secondTraversal, &retainedSize, "tag heap");
    if (!isOk(err)) return err;
    if (shouldStopExecution()) return MEMORY_AGENT_INTERRUPTED_ERROR;

    err = jvmti->SetTag(object, HELD_OBJECT_TAG);
    if (!isOk(err)) return err;

    jlong startObjectSize = 0;
    err = jvmti->GetObjectSize(object, &startObjectSize);
    if (!isOk(err)) return err;

    retainedSize += startObjectSize;

    return JVMTI_ERROR_NONE;
}

jvmtiError RetainedSizeAndHeldObjectsAction::estimateObjectSize(jobject &object, jlong &retainedSize, std::vector<jobject> &heldObjects) {
    jvmtiError err = traverseHeapForTheFirstTime(object);
    if (!isOk(err)) return err;
    if (shouldStopExecution()) return MEMORY_AGENT_INTERRUPTED_ERROR;

    err = traverseHeapFromStartObjectAndCountRetainedSize(object, retainedSize);
    if (!isOk(err)) return err;
    if (shouldStopExecution()) return MEMORY_AGENT_INTERRUPTED_ERROR;

    debug("collect held objects");
    updateProgress(85, "Collecting held objects...");
    return getObjectsByTags(jvmti, std::vector<jlong>{HELD_OBJECT_TAG}, heldObjects);
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
    return removeAllTagsFromHeap(jvmti, nullptr);
}
