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
#include "sizes/shallow_size_by_classes.h"
#include "sizes/retained_size_and_held_objects.h"
#include "sizes/retained_size_by_classes.h"
#include "sizes/retained_size_by_objects.h"

#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"

using namespace std;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"

static GlobalAgentData *gdata;

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
        cerr << "ERROR: Unable to access JVMTI!" << std::endl;
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

JNIEXPORT void JNICALL Agent_OnUnload(JavaVM *vm) {
    debug("on agent unload");
    delete gdata;
}

extern "C"
JNIEXPORT jboolean JNICALL Java_com_intellij_memory_agent_proxy_IdeaNativeAgentProxy_canEstimateObjectSize(
        JNIEnv *env,
        jclass thisClass) {
    return (uint8_t) 1;
}

extern "C"
JNIEXPORT jboolean JNICALL Java_com_intellij_memory_agent_proxy_IdeaNativeAgentProxy_canEstimateObjectsSizes(
        JNIEnv *env,
        jclass thisClass) {
    return (uint8_t) 1;
}

extern "C"
JNIEXPORT jboolean JNICALL Java_com_intellij_memory_agent_proxy_IdeaNativeAgentProxy_canFindPathsToClosestGcRoots(
        JNIEnv *env,
        jclass thisClass) {
    return (uint8_t) 1;
}

extern "C"
JNIEXPORT jboolean JNICALL Java_com_intellij_memory_agent_proxy_IdeaNativeAgentProxy_canGetRetainedSizeByClasses(
        JNIEnv *env,
        jclass thisClass) {
    return (uint8_t) 1;
}

extern "C"
JNIEXPORT jboolean JNICALL Java_com_intellij_memory_agent_proxy_IdeaNativeAgentProxy_canGetShallowSizeByClasses(
        JNIEnv *env,
        jclass thisClass) {
    return (uint8_t) 1;
}

extern "C"
JNIEXPORT jboolean JNICALL Java_com_intellij_memory_agent_proxy_IdeaNativeAgentProxy_isLoadedImpl(
        JNIEnv *env,
        jclass thisClass) {
    return (uint8_t) 1;
}

extern "C"
JNIEXPORT jlongArray JNICALL Java_com_intellij_memory_agent_proxy_IdeaNativeAgentProxy_estimateRetainedSize(
        JNIEnv *env,
        jclass thisClass,
        jobjectArray objects) {
    return RetainedSizeByObjectsAction(env, gdata->jvmti).runWithTimeout(std::chrono::seconds(100), objects);
}

extern "C"
JNIEXPORT jobjectArray JNICALL Java_com_intellij_memory_agent_proxy_IdeaNativeAgentProxy_size(
        JNIEnv *env,
        jclass thisClass,
        jobject object) {
    return RetainedSizeAndHeldObjectsAction(env, gdata->jvmti).runWithTimeout(std::chrono::seconds(100), object);
}

extern "C"
JNIEXPORT jobjectArray JNICALL Java_com_intellij_memory_agent_proxy_IdeaNativeAgentProxy_findPathsToClosestGcRoots(
        JNIEnv *env,
        jclass thisClass,
        jobject object,
        jint pathsNumber,
        jint objectsNumber) {
    return PathsToClosestGcRootsAction(env, gdata->jvmti).runWithTimeout(std::chrono::seconds(100), object, pathsNumber, objectsNumber);
}

extern "C"
JNIEXPORT jlongArray JNICALL Java_com_intellij_memory_agent_proxy_IdeaNativeAgentProxy_getShallowSizeByClasses(
        JNIEnv *env,
        jclass thisClass,
        jobjectArray classesArray) {
    return ShallowSizeByClassesAction(env, gdata->jvmti).runWithTimeout(std::chrono::seconds(100), classesArray);
}

extern "C"
JNIEXPORT jlongArray JNICALL Java_com_intellij_memory_agent_proxy_IdeaNativeAgentProxy_getRetainedSizeByClasses(
        JNIEnv *env,
        jclass thisClass,
        jobjectArray classesArray) {
    return RetainedSizeByClassesAction(env, gdata->jvmti).runWithTimeout(std::chrono::seconds(100), classesArray);
}

extern "C"
JNIEXPORT jobjectArray JNICALL Java_com_intellij_memory_agent_proxy_IdeaNativeAgentProxy_getShallowAndRetainedSizeByClasses(
        JNIEnv *env,
        jclass thisClass,
        jobjectArray classesArray) {
    return RetainedAndShallowSizeByClassesAction(env, gdata->jvmti).runWithTimeout(std::chrono::seconds(100), classesArray);
}

#pragma clang diagnostic pop
