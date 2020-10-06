// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#include "retained_size_by_objects.h"
#include "sizes_tags.h"
#include "retained_size_by_classes.h"

RetainedSizeByObjectsAction::RetainedSizeByObjectsAction(JNIEnv *env, jvmtiEnv *jvmti) : MemoryAgentTimedAction(env, jvmti) {

}

jvmtiError RetainedSizeByObjectsAction::calculateRetainedSizes(const std::vector<jobject> &objects, std::vector<jlong> &result) {
    std::set<jobject> unique(objects.begin(), objects.end());
    size_t count = objects.size();
    if (count != unique.size()) {
        fatal("Invalid argument: objects should be unique");
    }

    result.resize(static_cast<unsigned long>(count));
    return IterateThroughHeap(JVMTI_HEAP_FILTER_UNTAGGED, nullptr, visitObject, result.data(), "calculate retained sizes");
}

jvmtiError RetainedSizeByObjectsAction::createTagForObject(jobject object, size_t index) {
    jlong oldTag;
    jvmtiError err = jvmti->GetTag(object, &oldTag);
    if (!isOk(err)) return err;
    Tag *tag = Tag::create(index, createState(true, true, false, false));
    if (oldTag != 0 && !isTagWithNewInfo(oldTag)) {
        tagToPointer(oldTag)->array.extend(tag->array);
        delete tag;
    } else {
        err = jvmti->SetTag(object, pointerToTag(tag));
    }

    return err;
}

jvmtiError RetainedSizeByObjectsAction::createTagsForObjects(const std::vector<jobject> &objects) {
    for (size_t i = 0; i < objects.size(); i++) {
        jvmtiError err = createTagForObject(objects[i], i);
        if (!isOk(err)) return err;
        if (shouldStopExecution()) return MEMORY_AGENT_TIMEOUT_ERROR;
    }

    return JVMTI_ERROR_NONE;
}

jvmtiError RetainedSizeByObjectsAction::retagStartObjects(const std::vector<jobject> &objects) {
    std::vector<std::pair<jobject, size_t>> objectsWithNewInfo;
    for (size_t i = 0; i < objects.size(); i++) {
        jlong oldTag;
        jvmtiError err = jvmti->GetTag(objects[i], &oldTag);
        if (!isOk(err)) return err;
        if (shouldStopExecution()) return MEMORY_AGENT_TIMEOUT_ERROR;

        if (isTagWithNewInfo(oldTag)) {
            objectsWithNewInfo.emplace_back(objects[i], i);
        }
    }

    for (auto objectToIndex : objectsWithNewInfo) {
        jvmtiError err = createTagForObject(objectToIndex.first, objectToIndex.second);
        if (!isOk(err)) return err;
        if (shouldStopExecution()) return MEMORY_AGENT_TIMEOUT_ERROR;
    }

    return JVMTI_ERROR_NONE;
}

jvmtiError RetainedSizeByObjectsAction::tagHeap(const std::vector<jobject> &objects) {
    jvmtiError err = FollowReferences(0, nullptr, nullptr, getTagsWithNewInfo, nullptr, "find objects with new info");
    if (!isOk(err)) return err;
    if (shouldStopExecution()) return MEMORY_AGENT_TIMEOUT_ERROR;

    std::vector<jobject> taggedObjects;
    debug("collect objects with new info");
    err = getObjectsByTags(jvmti, std::vector<jlong>{pointerToTag(&Tag::TagWithNewInfo)}, taggedObjects);
    if (!isOk(err)) return err;
    if (shouldStopExecution()) return MEMORY_AGENT_TIMEOUT_ERROR;

    err = retagStartObjects(objects);
    if (!isOk(err)) return err;
    if (shouldStopExecution()) return MEMORY_AGENT_TIMEOUT_ERROR;

    err = FollowReferences(0, nullptr, nullptr, visitReference, nullptr, "tag heap");
    if (!isOk(err)) return err;
    if (shouldStopExecution()) return MEMORY_AGENT_TIMEOUT_ERROR;

    return walkHeapFromObjects(jvmti, taggedObjects, finishTime);
}

jvmtiError RetainedSizeByObjectsAction::estimateObjectsSizes(const std::vector<jobject> &objects, std::vector<jlong> &result) {
    jvmtiError err = createTagsForObjects(objects);
    if (!isOk(err)) return err;

    err = tagHeap(objects);
    if (!isOk(err)) return err;

    return calculateRetainedSizes(objects, result);
}

jlongArray RetainedSizeByObjectsAction::executeOperation(jobjectArray objects) {
    debug("start estimate objects sizes");
    debug("convert java array to vector");
    std::vector<jobject> javaObjects;
    fromJavaArray(env, objects, javaObjects);
    std::vector<jlong> result;
    jvmtiError err = estimateObjectsSizes(javaObjects, result);
    if (!isOk(err)) {
        handleError(jvmti, err, "Could not estimate objects size");
        return env->NewLongArray(0);
    }

    return toJavaArray(env, result);
}

jvmtiError RetainedSizeByObjectsAction::cleanHeap() {
    jvmtiError err = IterateThroughHeap(0, nullptr, clearTag, nullptr, "clear tags");

    if (sizesTagBalance != 0) {
        fatal("MEMORY LEAK FOUND!");
    }

    return err;
}
