// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#include <vector>

#include "retained_size_via_dominator_tree.h"
#include "dominator_tree.h"

#define VISITED_TAG (-1)
#define MOCK_REFERRER_TAG (-2)
#define CLASS_TAG (-3)
#define OBJECT_OF_CLASS_TAG (-4)

class SizesViaDominatorTreeHeapDumpInfo {
public:
    SizesViaDominatorTreeHeapDumpInfo() : graph(1), sizes(1) {}

    jvmtiError initAndSetTagsForObjects(JNIEnv *env, jvmtiEnv *jvmti, jobjectArray objects) {
        jvmtiError err = JVMTI_ERROR_NONE;
        jsize size = env->GetArrayLength(objects);
        for (jsize i = 0; i < size; i++) {
            jobject object = env->GetObjectArrayElement(objects, i);
            jlong objectTag;
            err = jvmti->GetTag(object, &objectTag);
            if (!isOk(err)) return err;
            if (objectTag != 0 && objectTag != OBJECT_OF_CLASS_TAG)
                continue;

            jlong tag = currentTag++;
            err = jvmti->SetTag(object, tag);
            if (!isOk(err)) return err;

            jlong objectSize;
            err = jvmti->GetObjectSize(object, &objectSize);
            if (!isOk(err)) return err;

            graph.emplace_back();
            sizes.push_back(objectSize);
        }
        lastStartTag = currentTag - 1;
        wasVisitedDuringFirstTraversal.resize(currentTag);

        return err;
    }

    void setUpNeighboursForMasterNode() {
        for (jint i = 1; i <= lastStartTag; i++) {
            if (wasVisitedDuringFirstTraversal[i]) {
                graph[0].push_back(i);
            }
        }
    }

    jlong addNewVertex(jlong size) {
        sizes.push_back(size);
        graph.emplace_back();
        return currentTag++;
    }

    void addNeighbour(jlong parent, jlong neighbour) {
        if (parent >= graph.size()) {
            throw std::invalid_argument("Parent number exceeds graph size");
        }
        graph[parent].push_back(neighbour);
    }

    void visitStartVertex(jlong vertex) {
        if (vertex >= wasVisitedDuringFirstTraversal.size()) {
            throw std::invalid_argument("Vertex number exceeds number of start vertices");
        }
        wasVisitedDuringFirstTraversal[vertex] = true;
    }

public:
    std::vector<std::vector<jlong>> graph;
    std::vector<jlong> sizes;
    std::vector<bool> wasVisitedDuringFirstTraversal;
    jlong lastStartTag = 1;
    jlong currentTag = 1;
};

namespace {
    jint JNICALL collectObjects(jvmtiHeapReferenceKind refKind, const jvmtiHeapReferenceInfo *refInfo, jlong classTag,
                                jlong referrerClassTag, jlong size, jlong *tagPtr,
                                jlong *referrerTagPtr, jint length, void *userData) {
        if (refKind == JVMTI_HEAP_REFERENCE_JNI_LOCAL || refKind == JVMTI_HEAP_REFERENCE_JNI_GLOBAL) {
            return 0;
        } else if (classTag == CLASS_TAG) {
            *tagPtr = OBJECT_OF_CLASS_TAG;
        }
        return JVMTI_VISIT_OBJECTS;
    }

    jint JNICALL firstTraversal(jvmtiHeapReferenceKind refKind, const jvmtiHeapReferenceInfo *refInfo, jlong classTag,
                                jlong referrerClassTag, jlong size, jlong *tagPtr,
                                jlong *referrerTagPtr, jint length, void *userData) {
        auto *info = reinterpret_cast<SizesViaDominatorTreeHeapDumpInfo *>(userData);
        if (refKind == JVMTI_HEAP_REFERENCE_JNI_LOCAL || refKind == JVMTI_HEAP_REFERENCE_JNI_GLOBAL) {
            return 0;
        } else if (*tagPtr == 0) {
            *tagPtr = VISITED_TAG;
        } else if (*tagPtr <= info->lastStartTag) {
            if (*tagPtr > 0) {
                info->visitStartVertex(*tagPtr);
            }
            return 0;
        }

        return JVMTI_VISIT_OBJECTS;
    }

    jint JNICALL secondTraversal(jvmtiHeapReferenceKind refKind, const jvmtiHeapReferenceInfo *refInfo, jlong classTag,
                                 jlong referrerClassTag, jlong size, jlong *tagPtr,
                                 jlong *referrerTagPtr, jint length, void *userData) {
        if (*referrerTagPtr == MOCK_REFERRER_TAG) {
            return JVMTI_VISIT_OBJECTS;
        }

        auto *info = reinterpret_cast<SizesViaDominatorTreeHeapDumpInfo *>(userData);
        if (*tagPtr == VISITED_TAG || refKind == JVMTI_HEAP_REFERENCE_JNI_LOCAL || refKind == JVMTI_HEAP_REFERENCE_JNI_GLOBAL) {
            return 0;
        } else if (*tagPtr == 0) {
            *tagPtr = info->addNewVertex(size);
            info->addNeighbour(*referrerTagPtr, *tagPtr);
        } else {
            info->addNeighbour(*referrerTagPtr, *tagPtr);
            return 0;
        }

        return JVMTI_VISIT_OBJECTS;
    }
}

jobjectArray RetainedSizeViaDominatorTreeAction::executeOperation(jobjectArray objects) {
    SizesViaDominatorTreeHeapDumpInfo info;
    jvmtiError err = info.initAndSetTagsForObjects(env, jvmti, objects);
    if (err != JVMTI_ERROR_NONE) return nullptr;

    // We set a tag for the input array to ignore it during traversal
    err = jvmti->SetTag(objects, MOCK_REFERRER_TAG);
    if (err != JVMTI_ERROR_NONE) return nullptr;

    progressManager.updateProgress(10, "Traversing heap for the first time...");
    logger::resetTimer();
    err = FollowReferences(0, nullptr, nullptr, firstTraversal, &info, "first traversal");
    logger::logPassedTime();
    if (err != JVMTI_ERROR_NONE || shouldStopExecution()) return nullptr;

    logger::resetTimer();
    progressManager.updateProgress(60, "Traversing heap for the second time...");
    err = FollowReferences(0, nullptr, objects, secondTraversal, &info, "second traversal");
    logger::logPassedTime();
    if (err != JVMTI_ERROR_NONE || shouldStopExecution()) return nullptr;

    progressManager.updateProgress(80, "Calculating retained size...");
    info.setUpNeighboursForMasterNode();
    std::vector<jlong> retained_sizes = calculateRetainedSizes(info.graph, info.sizes);

    progressManager.updateProgress(95, "Extracting answer...");
    jsize size = env->GetArrayLength(objects);
    std::vector<jlong> shallowSizes;
    std::vector<jlong> retainedSizes;
    retainedSizes.reserve(size);
    shallowSizes.reserve(size);
    for (jsize i = 0; i < size; i++) {
        jobject object = env->GetObjectArrayElement(objects, i);
        jlong tag;
        jvmti->GetTag(object, &tag);
        retainedSizes.push_back(retained_sizes[tag]);
        shallowSizes.push_back(info.sizes[tag]);
    }

    jclass langObject = env->FindClass("java/lang/Object");
    jobjectArray result = env->NewObjectArray(2, langObject, nullptr);
    env->SetObjectArrayElement(result, 0, toJavaArray(env, shallowSizes));
    env->SetObjectArrayElement(result, 1, toJavaArray(env, retainedSizes));
    if (!isOk(err)) {
        handleError(jvmti, err, "Could not estimate retained size by classes");
        return nullptr;
    }
    return result;
}

jvmtiError RetainedSizeViaDominatorTreeAction::cleanHeap() {
    return removeAllTagsFromHeap(jvmti, nullptr);
}

RetainedSizeViaDominatorTreeAction::RetainedSizeViaDominatorTreeAction(JNIEnv *env, jvmtiEnv *jvmti, jobject object) : 
	MemoryAgentAction(env, jvmti, object) {
}

jobjectArray RetainedSizeByClassViaDominatorTreeAction::executeOperation(jobject classRef, jlong objectsLimit) {
    jvmtiError err = jvmti->SetTag(classRef, CLASS_TAG);
    if (err != JVMTI_ERROR_NONE) return nullptr;

    err = FollowReferences(0, nullptr, nullptr, collectObjects, nullptr, "collect objects of class");
    if (err != JVMTI_ERROR_NONE || shouldStopExecution()) return nullptr;

    std::vector<jobject> objectsOfClass;
    err = getObjectsByTags(jvmti, std::vector<jlong>{OBJECT_OF_CLASS_TAG}, objectsOfClass);
    if (err != JVMTI_ERROR_NONE || shouldStopExecution()) return nullptr;

    // Constructing the master node
    jclass langObject = env->FindClass("java/lang/Object");
    jobjectArray objects = env->NewObjectArray(objectsOfClass.size(), langObject, nullptr);
    for (int i = 0; i < objectsOfClass.size(); i++) {
        env->SetObjectArrayElement(objects, i, objectsOfClass[i]);
    }

    err = jvmti->SetTag(classRef, 0);
    if (err != JVMTI_ERROR_NONE) return nullptr;

    SizesViaDominatorTreeHeapDumpInfo info;
    err = info.initAndSetTagsForObjects(env, jvmti, objects);
    if (err != JVMTI_ERROR_NONE) return nullptr;

    // We set a tag for the input array to ignore it during traversal
    err = jvmti->SetTag(objects, MOCK_REFERRER_TAG);
    if (err != JVMTI_ERROR_NONE) return nullptr;

    progressManager.updateProgress(10, "Traversing heap for the first time...");
    logger::resetTimer();
    err = FollowReferences(0, nullptr, nullptr, firstTraversal, &info, "first traversal");
    logger::logPassedTime();
    if (err != JVMTI_ERROR_NONE || shouldStopExecution()) return nullptr;

    logger::resetTimer();
    progressManager.updateProgress(60, "Traversing heap for the second time...");
    err = FollowReferences(0, nullptr, objects, secondTraversal, &info, "second traversal");
    logger::logPassedTime();
    if (err != JVMTI_ERROR_NONE || shouldStopExecution()) return nullptr;

    progressManager.updateProgress(80, "Calculating retained size...");
    info.setUpNeighboursForMasterNode();
    std::vector<jlong> retainedSizes = calculateRetainedSizes(info.graph, info.sizes);

    // Sort objects by their retained size
    std::sort(objectsOfClass.begin(), objectsOfClass.end(), [&](jobject a, jobject b) {
        jlong tagA;
        jvmti->GetTag(a, &tagA);
        jlong tagB;
        jvmti->GetTag(b, &tagB);
        return retainedSizes[tagA] > retainedSizes[tagB];
    });

    progressManager.updateProgress(95, "Extracting answer...");
    size_t size = std::min((size_t)objectsLimit, objectsOfClass.size());
    std::vector<jlong> shallowSizes;
    std::vector<jlong> sortedRetainedSizes;
    sortedRetainedSizes.reserve(size);
    shallowSizes.reserve(size);
    for (size_t i = 0; i < size; i++) {
        jobject object = objectsOfClass[i];
        jlong tag;
        jvmti->GetTag(object, &tag);
        sortedRetainedSizes.push_back(retainedSizes[tag]);
        shallowSizes.push_back(info.sizes[tag]);
    }

    jobjectArray result = env->NewObjectArray(3, langObject, nullptr);
    objects = env->NewObjectArray(size, langObject, nullptr);
    for (int i = 0; i < size; i++) {
        env->SetObjectArrayElement(objects, i, objectsOfClass[i]);
    }
    env->SetObjectArrayElement(result, 0, objects);
    env->SetObjectArrayElement(result, 1, toJavaArray(env, shallowSizes));
    env->SetObjectArrayElement(result, 2, toJavaArray(env, sortedRetainedSizes));
    if (!isOk(err)) {
        handleError(jvmti, err, "Could not estimate retained size by classes");
        return nullptr;
    }
    return result;
}

jvmtiError RetainedSizeByClassViaDominatorTreeAction::cleanHeap() {
    return removeAllTagsFromHeap(jvmti, nullptr);
}

RetainedSizeByClassViaDominatorTreeAction::RetainedSizeByClassViaDominatorTreeAction(JNIEnv *env, jvmtiEnv *jvmti, jobject object) :
        MemoryAgentAction(env, jvmti, object) {
}
