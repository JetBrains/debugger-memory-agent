// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#include "retained_size_by_objects_merged.h"
#include "sizes_tags.h"
#include "retained_size_by_classes.h"

#define START_TAG 1
#define VISITED_TAG 2

jint JNICALL firstTravers(jvmtiHeapReferenceKind refKind, const jvmtiHeapReferenceInfo *refInfo, jlong classTag,
                            jlong referrerClassTag, jlong size, jlong *tagPtr,
                            jlong *referrerTagPtr, jint length, void *userData) {
    if (refKind == JVMTI_HEAP_REFERENCE_JNI_LOCAL || refKind == JVMTI_HEAP_REFERENCE_JNI_GLOBAL) {
        return 0;
    } else if (*tagPtr == START_TAG) {
        return 0;
    } else {
        *tagPtr = VISITED_TAG;
    }
    return JVMTI_VISIT_OBJECTS;
}

jint JNICALL secondTravers(jvmtiHeapReferenceKind refKind, const jvmtiHeapReferenceInfo *refInfo, jlong classTag,
                            jlong referrerClassTag, jlong size, jlong *tagPtr,
                            jlong *referrerTagPtr, jint length, void *userData) {
    if (refKind == JVMTI_HEAP_REFERENCE_JNI_LOCAL || refKind == JVMTI_HEAP_REFERENCE_JNI_GLOBAL) {
        return 0;
    } else if (*tagPtr == VISITED_TAG){
        return 0;
    } else if (*tagPtr != VISITED_TAG) {
           // *reinterpret_cast<jlong *>(userData) += size;
            *tagPtr = START_TAG;
    }
    return JVMTI_VISIT_OBJECTS;
}

RetainedSizeByObjectsMergedAction::RetainedSizeByObjectsMergedAction(JNIEnv *env, jvmtiEnv *jvmti, jobject object) : MemoryAgentAction(env, jvmti, object) {

}

jvmtiError RetainedSizeByObjectsMergedAction::traverseHeapForTheFirstTime(std::vector<jobject> &objects) {
    for(const auto& object: objects) {
        jvmtiError err = jvmti->SetTag(object, START_TAG);
        if (!isOk(err)) return err;
        if (shouldStopExecution()) return MEMORY_AGENT_INTERRUPTED_ERROR;
    }
    progressManager.updateProgress(10, "Traversing heap for the first time...");
    return FollowReferences(0, nullptr, nullptr, firstTravers, nullptr, "tag heap");
}

jvmtiError RetainedSizeByObjectsMergedAction::traverseHeapFromStartObject(std::vector<jobject> &objects) {
    progressManager.updateProgress(45, "Traversing heap for the second time...");
    for(const auto& object: objects) {
        jvmtiError err = FollowReferences(0, nullptr, object, secondTravers, nullptr, "tag heap");
        if (!isOk(err)) return err;
        if (shouldStopExecution()) return MEMORY_AGENT_INTERRUPTED_ERROR;
    }
    return JVMTI_ERROR_NONE;
}

jvmtiError RetainedSizeByObjectsMergedAction::countRetainedSize(jlong &retainedSize){
    std::vector<jobject> objects;
    retainedSize = 0;
    jvmtiError err = getObjectsByTags(this->jvmti, std::vector<jlong>{START_TAG}, objects);
    if (!isOk(err)) return err;
    if (shouldStopExecution()) return MEMORY_AGENT_INTERRUPTED_ERROR;
    for (const auto& object:objects){
        jlong startObjectSize = 0;
        err = jvmti->GetObjectSize(object, &startObjectSize);
        if (!isOk(err)) return err;
        retainedSize += startObjectSize;
    }
    return err;
}

jvmtiError RetainedSizeByObjectsMergedAction::estimateObjectsSize(std::vector<jobject> &objects, jlong &retainedSize) {
    jvmtiError err = traverseHeapForTheFirstTime(objects);
    std::cout << "finish first traversal" << std::endl;
    if (!isOk(err)) return err;
    if (shouldStopExecution()) return MEMORY_AGENT_INTERRUPTED_ERROR;
    err = traverseHeapFromStartObject(objects);
    std::cout << "finish second traversal" << std::endl;
    if (!isOk(err)) return err;
    if (shouldStopExecution()) return MEMORY_AGENT_INTERRUPTED_ERROR;
    return countRetainedSize(retainedSize);
}

jlongArray  RetainedSizeByObjectsMergedAction::executeOperation(jobjectArray objects) {
    std::vector<jobject> objects_v;
    fromJavaArray(env, objects, objects_v);
    jlong retainedSize;
    jvmtiError err = estimateObjectsSize(objects_v, retainedSize);
    std::cout << "got res " << retainedSize <<  std::endl;
    if (!isOk(err)) {
        handleError(jvmti, err, "Could not estimate object size");
    }
    std::vector<jlong> res;
    res.push_back(retainedSize);
    return toJavaArray(env, res);
}

jvmtiError RetainedSizeByObjectsMergedAction::cleanHeap() {
    return removeAllTagsFromHeap(jvmti, nullptr);
}
