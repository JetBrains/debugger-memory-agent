// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#include <jvmti.h>
#include <queue>
#include <iostream>
#include <unordered_map>
#include <cstring>
#include "log.h"
#include "global_data.h"
#include "utils.h"
#include "roots/paths_to_closest_gc_roots.h"
#include "reachability/objects_of_class_in_heap.h"
#include "sizes/shallow_size_by_classes.h"
#include "sizes/retained_size_and_held_objects.h"
#include "sizes/retained_size_by_classes.h"
#include "sizes/retained_size_by_objects.h"
#include "allocation_sampling.h"

#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"

static GlobalAgentData *gdata = nullptr;
static bool canSampleAllocations = false;

extern void handleOptions(const char *);

static void setRequiredCapabilities(jvmtiEnv *jvmti, jvmtiCapabilities &effective) {
    jvmtiCapabilities potential;
    std::memset(&potential, 0, sizeof(jvmtiCapabilities));
    std::memset(&effective, 0, sizeof(jvmtiCapabilities));

    jvmti->GetPotentialCapabilities(&potential);

    if (potential.can_tag_objects) {
        effective.can_tag_objects = 1;
    }
    if (potential.can_generate_object_free_events) {
        effective.can_generate_object_free_events = 1;
    }
    if (potential.can_generate_sampled_object_alloc_events) {
        canSampleAllocations = true;
        effective.can_generate_sampled_object_alloc_events = 1;
    }
}

JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM *jvm, char *options, void *reserved) {
    handleOptions(options);

    debug("on agent load");
    jvmtiEnv *jvmti = nullptr;
    jint result = jvm->GetEnv((void **) &jvmti, JVMTI_VERSION_1_0);
    if (result != JNI_OK || jvmti == nullptr) {
        std::cerr << "ERROR: Unable to access JVMTI!" << std::endl;
        return result;
    }

    debug("set capabilities");
    jvmtiCapabilities capabilities;
    setRequiredCapabilities(jvmti, capabilities);
    jvmtiError error = jvmti->AddCapabilities(&capabilities);
    if (error != JVMTI_ERROR_NONE) {
        handleError(jvmti, error, "Could not set capabilities");
        return JNI_ERR;
    }

    if (canSampleAllocations) {
        error = jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_SAMPLED_OBJECT_ALLOC, NULL);
        if (error != JVMTI_ERROR_NONE) {
            handleError(jvmti, error, "Could not set allocation sampling notification mode");
            return JNI_ERR;
        }
    }

    debug("set callbacks");
    jvmtiEventCallbacks callbacks;
    std::memset(&callbacks, 0, sizeof(jvmtiEventCallbacks));
    callbacks.SampledObjectAlloc = SampledObjectAlloc;
    jvmti->SetEventCallbacks(&callbacks, sizeof(jvmtiEventCallbacks));

    gdata = new GlobalAgentData();
    gdata->jvmti = jvmti;
    debug("initializing done");
    return JNI_OK;
}

JNIEXPORT jint JNICALL Agent_OnAttach(JavaVM *vm, char *options, void *reserved) {
    delete gdata;
    return Agent_OnLoad(vm, options, reserved);
}

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    delete gdata;
    Agent_OnLoad(vm, nullptr, reserved);
    return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL Agent_OnUnload(JavaVM *vm) {
    debug("on agent unload");
    delete gdata;
}

extern "C"
JNIEXPORT jboolean JNICALL Java_com_intellij_memory_agent_IdeaNativeAgentProxy_canEstimateObjectSize(
        JNIEnv *env,
        jobject thisObject) {
    return (jboolean) 1;
}

extern "C"
JNIEXPORT jboolean JNICALL Java_com_intellij_memory_agent_IdeaNativeAgentProxy_canEstimateObjectsSizes(
        JNIEnv *env,
        jobject thisObject) {
    return (jboolean) 1;
}

extern "C"
JNIEXPORT jboolean JNICALL Java_com_intellij_memory_agent_IdeaNativeAgentProxy_canFindPathsToClosestGcRoots(
        JNIEnv *env,
        jobject thisObject) {
    return (jboolean) 1;
}

extern "C"
JNIEXPORT jboolean JNICALL Java_com_intellij_memory_agent_IdeaNativeAgentProxy_canGetRetainedSizeByClasses(
        JNIEnv *env,
        jobject thisObject) {
    return (jboolean) 1;
}

extern "C"
JNIEXPORT jboolean JNICALL Java_com_intellij_memory_agent_IdeaNativeAgentProxy_canGetShallowSizeByClasses(
        JNIEnv *env,
        jobject thisObject) {
    return (jboolean) 1;
}

extern "C"
JNIEXPORT jboolean JNICALL Java_com_intellij_memory_agent_IdeaNativeAgentProxy_isLoadedImpl(
        JNIEnv *env,
        jclass thisClass) {
    return gdata != nullptr;
}

extern "C"
JNIEXPORT jobjectArray JNICALL Java_com_intellij_memory_agent_IdeaNativeAgentProxy_estimateRetainedSize(
        JNIEnv *env,
        jobject thisObject,
        jobjectArray objects) {
    return RetainedSizeByObjectsAction(env, gdata->jvmti, thisObject).run(objects);
}

extern "C"
JNIEXPORT jobjectArray JNICALL Java_com_intellij_memory_agent_IdeaNativeAgentProxy_size(
        JNIEnv *env,
        jobject thisObject,
        jobject object) {
    return RetainedSizeAndHeldObjectsAction(env, gdata->jvmti, thisObject).run(object);
}

extern "C"
JNIEXPORT jobjectArray JNICALL Java_com_intellij_memory_agent_IdeaNativeAgentProxy_findPathsToClosestGcRoots(
        JNIEnv *env,
        jobject thisObject,
        jobject object,
        jint pathsNumber,
        jint objectsNumber) {
    return PathsToClosestGcRootsAction(env, gdata->jvmti, thisObject).run(object, pathsNumber, objectsNumber);
}

extern "C"
JNIEXPORT jobjectArray JNICALL Java_com_intellij_memory_agent_IdeaNativeAgentProxy_getShallowSizeByClasses(
        JNIEnv *env,
        jobject thisObject,
        jobjectArray classesArray) {
    return ShallowSizeByClassesAction(env, gdata->jvmti, thisObject).run(classesArray);
}

extern "C"
JNIEXPORT jobjectArray JNICALL Java_com_intellij_memory_agent_IdeaNativeAgentProxy_getRetainedSizeByClasses(
        JNIEnv *env,
        jobject thisObject,
        jobjectArray classesArray) {
    return RetainedSizeByClassesAction(env, gdata->jvmti, thisObject).run(classesArray);
}

extern "C"
JNIEXPORT jobjectArray JNICALL Java_com_intellij_memory_agent_IdeaNativeAgentProxy_getShallowAndRetainedSizeByClasses(
        JNIEnv *env,
        jobject thisObject,
        jobjectArray classesArray) {
    return RetainedAndShallowSizeByClassesAction(env, gdata->jvmti, thisObject).run(classesArray);
}

extern "C"
JNIEXPORT jobjectArray JNICALL Java_com_intellij_memory_agent_IdeaNativeAgentProxy_getFirstReachableObject(
        JNIEnv *env,
        jobject thisObject,
        jobject startObject,
        jobjectArray suspectClass) {
    return GetFirstReachableObjectOfClassAction(env, gdata->jvmti, thisObject).run(startObject, suspectClass);
}

extern "C"
JNIEXPORT jobjectArray JNICALL Java_com_intellij_memory_agent_IdeaNativeAgentProxy_getAllReachableObjects(
        JNIEnv *env,
        jobject thisObject,
        jobject startObject,
        jobject suspectClass) {
    return GetAllReachableObjectsOfClassAction(env, gdata->jvmti, thisObject).run(startObject, suspectClass);
}

extern "C"
JNIEXPORT jint JNICALL Java_com_intellij_memory_agent_IdeaNativeAgentProxy_addAllocationListener(
        JNIEnv *env,
        jclass thisClass, 
        jobject listener,
        jobjectArray trackedClasses) {
    if (!canSampleAllocations) {
        return (jint) -1;
    }
    size_t index = listenersHolder.addListener(env, listener, fromJavaArray(env, trackedClasses));
    return (jint) index;
}

extern "C"
JNIEXPORT jboolean JNICALL Java_com_intellij_memory_agent_IdeaNativeAgentProxy_setHeapSamplingInterval(
        JNIEnv *env,
        jclass thisClass,
        jlong interval) {
    if (!canSampleAllocations || JVMTI_ERROR_NONE != gdata->jvmti->SetHeapSamplingInterval(interval)) {
        return (jboolean) 0;
    }

    return (jboolean) 1;
}

extern "C"
JNIEXPORT void JNICALL Java_com_intellij_memory_agent_IdeaNativeAgentProxy_removeAllocationListener(
        JNIEnv *env,
        jclass thisClass,
        jint index) {
    listenersHolder.removeListener(env, index);
}

#pragma clang diagnostic pop
