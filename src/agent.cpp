// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#include <jvmti.h>
#include <queue>
#include <iostream>
#include <unordered_map>
#include <cstring>
#include "log.h"
#include "types.h"
#include "utils.h"
#include "roots/paths_to_closest_gc_roots.h"
#include "reachability/objects_of_class_in_heap.h"
#include "sizes/shallow_size_by_classes.h"
#include "sizes/retained_size_and_held_objects.h"
#include "sizes/retained_size_by_classes.h"
#include "sizes/retained_size_by_objects.h"

#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"

static GlobalAgentData *gdata = nullptr;

static void requiredCapabilities(jvmtiEnv *jvmti, jvmtiCapabilities &effective) {
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
}

static jboolean canAddAndRemoveTags() {
    jvmtiCapabilities capabilities;
    std::memset(&capabilities, 0, sizeof(jvmtiCapabilities));
    gdata->jvmti->GetCapabilities(&capabilities);
    return static_cast<jboolean>(capabilities.can_tag_objects && capabilities.can_generate_object_free_events);
}

extern void handleOptions(const char *);

static void JNICALL ObjectFreeCallback(jvmtiEnv *jvmti_env, jlong tag) {
    debug("tagged object has been freed");
    delete reinterpret_cast<const char *>(tag);
}

JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM *jvm, char *options, void *reserved) {
    jvmtiEnv *jvmti = nullptr;
    jvmtiCapabilities capabilities;
    jvmtiError error;

    handleOptions(options);

    debug("on agent load");
    jint result = jvm->GetEnv((void **) &jvmti, JVMTI_VERSION_1_0);
    if (result != JNI_OK || jvmti == nullptr) {
        std::cerr << "ERROR: Unable to access JVMTI!" << std::endl;
        return result;
    }

    requiredCapabilities(jvmti, capabilities);

    debug("set capabilities");
    error = jvmti->AddCapabilities(&capabilities);
    if (error != JVMTI_ERROR_NONE) {
        handleError(jvmti, error, "Could not set capabilities");
        return JNI_ERR;
    }

    debug("set callbacks");
    jvmtiEventCallbacks callbacks;
    std::memset(&callbacks, 0, sizeof(jvmtiEventCallbacks));
    callbacks.ObjectFree = ObjectFreeCallback;
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

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    delete gdata;
    Agent_OnLoad(vm, nullptr, reserved);
    return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL Agent_OnUnload(JavaVM *vm) {
    debug("on agent unload");
    delete gdata;
}

extern "C"
JNIEXPORT jboolean JNICALL Java_com_intellij_memory_agent_proxy_IdeaNativeAgentProxy_canEstimateObjectSize(
        JNIEnv *env,
        jobject thisObject) {
    return (uint8_t) 1;
}

extern "C"
JNIEXPORT jboolean JNICALL Java_com_intellij_memory_agent_proxy_IdeaNativeAgentProxy_canEstimateObjectsSizes(
        JNIEnv *env,
        jobject thisObject) {
    return (uint8_t) 1;
}

extern "C"
JNIEXPORT jboolean JNICALL Java_com_intellij_memory_agent_proxy_IdeaNativeAgentProxy_canFindPathsToClosestGcRoots(
        JNIEnv *env,
        jobject thisObject) {
    return (uint8_t) 1;
}

extern "C"
JNIEXPORT jboolean JNICALL Java_com_intellij_memory_agent_proxy_IdeaNativeAgentProxy_canGetRetainedSizeByClasses(
        JNIEnv *env,
        jobject thisObject) {
    return (uint8_t) 1;
}

extern "C"
JNIEXPORT jboolean JNICALL Java_com_intellij_memory_agent_proxy_IdeaNativeAgentProxy_canGetShallowSizeByClasses(
        JNIEnv *env,
        jobject thisObject) {
    return (uint8_t) 1;
}

extern "C"
JNIEXPORT jboolean JNICALL Java_com_intellij_memory_agent_proxy_IdeaNativeAgentProxy_isLoadedImpl(
        JNIEnv *env,
        jobject thisObject) {
    return gdata != nullptr;
}

extern "C"
JNIEXPORT jobjectArray JNICALL Java_com_intellij_memory_agent_proxy_IdeaNativeAgentProxy_estimateRetainedSize(
        JNIEnv *env,
        jobject thisObject,
        jobjectArray objects) {
    return RetainedSizeByObjectsAction(env, gdata->jvmti, thisObject).run(objects);
}

extern "C"
JNIEXPORT jobjectArray JNICALL Java_com_intellij_memory_agent_proxy_IdeaNativeAgentProxy_size(
        JNIEnv *env,
        jobject thisObject,
        jobject object) {
    return RetainedSizeAndHeldObjectsAction(env, gdata->jvmti, thisObject).run(object);
}

extern "C"
JNIEXPORT jobjectArray JNICALL Java_com_intellij_memory_agent_proxy_IdeaNativeAgentProxy_findPathsToClosestGcRoots(
        JNIEnv *env,
        jobject thisObject,
        jobject object,
        jint pathsNumber,
        jint objectsNumber) {
    return PathsToClosestGcRootsAction(env, gdata->jvmti, thisObject).run(object, pathsNumber, objectsNumber);
}

extern "C"
JNIEXPORT jobjectArray JNICALL Java_com_intellij_memory_agent_proxy_IdeaNativeAgentProxy_getShallowSizeByClasses(
        JNIEnv *env,
        jobject thisObject,
        jobjectArray classesArray) {
    return ShallowSizeByClassesAction(env, gdata->jvmti, thisObject).run(classesArray);
}

extern "C"
JNIEXPORT jobjectArray JNICALL Java_com_intellij_memory_agent_proxy_IdeaNativeAgentProxy_getRetainedSizeByClasses(
        JNIEnv *env,
        jobject thisObject,
        jobjectArray classesArray) {
    return RetainedSizeByClassesAction(env, gdata->jvmti, thisObject).run(classesArray);
}

extern "C"
JNIEXPORT jobjectArray JNICALL Java_com_intellij_memory_agent_proxy_IdeaNativeAgentProxy_getShallowAndRetainedSizeByClasses(
        JNIEnv *env,
        jobject thisObject,
        jobjectArray classesArray) {
    return RetainedAndShallowSizeByClassesAction(env, gdata->jvmti, thisObject).run(classesArray);
}

extern "C"
JNIEXPORT jobjectArray JNICALL Java_com_intellij_memory_agent_proxy_IdeaNativeAgentProxy_getFirstReachableObject(
        JNIEnv *env,
        jobject thisObject,
        jobject startObject,
        jobjectArray suspectClass) {
    return GetFirstReachableObjectOfClassAction(env, gdata->jvmti, thisObject).run(startObject, suspectClass);
}

extern "C"
JNIEXPORT jobjectArray JNICALL Java_com_intellij_memory_agent_proxy_IdeaNativeAgentProxy_getAllReachableObjects(
        JNIEnv *env,
        jobject thisObject,
        jobject startObject,
        jobject suspectClass) {
    return GetAllReachableObjectsOfClassAction(env, gdata->jvmti, thisObject).run(startObject, suspectClass);
}

#pragma clang diagnostic pop
