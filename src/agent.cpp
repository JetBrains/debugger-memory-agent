// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#include <jvmti.h>
#include <vector>
#include <queue>
#include <cstdio>
#include <iostream>
#include <memory.h>
#include <set>
#include <unordered_map>
#include <cstring>
#include "types.h"
#include "utils.h"
#include "object_size.h"
#include "gc_roots.h"

#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"

using namespace std;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"

static GlobalAgentData *gdata;

static void required_capabilities(jvmtiEnv *jvmti, jvmtiCapabilities &effective) {
    jvmtiCapabilities potential;
    std::memset(&potential, 0, sizeof(jvmtiCapabilities));
    std::memset(&effective, 0, sizeof(jvmtiCapabilities));

    jvmti->GetPotentialCapabilities(&potential);

    if (potential.can_tag_objects) {
        effective.can_tag_objects = 1;
    }
}

static jboolean can_tag_objects() {
    jvmtiCapabilities capabilities;
    std::memset(&capabilities, 0, sizeof(jvmtiCapabilities));
    gdata->jvmti->GetCapabilities(&capabilities);
    return static_cast<jboolean>(capabilities.can_tag_objects);
}

JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM *jvm, char *options, void *reserved) {
    jvmtiEnv *jvmti = nullptr;
    jvmtiCapabilities capabilities;
    jvmtiError error;

    jint result = jvm->GetEnv((void **) &jvmti, JVMTI_VERSION_1_0);
    if (result != JNI_OK || jvmti == nullptr) {
        cerr << "ERROR: Unable to access JVMTI!" << std::endl;
        return result;
    }

    required_capabilities(jvmti, capabilities);
    error = jvmti->AddCapabilities(&capabilities);
    handleError(jvmti, error, "Could not set capabilities");

    gdata = new GlobalAgentData();
    gdata->jvmti = jvmti;
    return JNI_OK;
}

JNIEXPORT void JNICALL Agent_OnUnload(JavaVM *vm) {
    delete gdata;
}

#pragma clang diagnostic pop

extern "C"
JNIEXPORT jboolean JNICALL Java_com_intellij_memory_agent_proxy_IdeaNativeAgentProxy_canEstimateObjectSize(
        JNIEnv *env,
        jclass thisClass) {
    return (uint8_t) 1;
}

extern "C"
JNIEXPORT jlong JNICALL Java_com_intellij_memory_agent_proxy_IdeaNativeAgentProxy_size(
        JNIEnv *env,
        jclass thisClass,
        jobject object) {
    return estimateObjectSize(env, gdata->jvmti, thisClass, object);
}

extern "C"
JNIEXPORT jboolean JNICALL Java_com_intellij_memory_agent_proxy_IdeaNativeAgentProxy_canFindGcRoots(
        JNIEnv *env,
        jclass thisClass,
        jobject object) {
    return can_tag_objects();
}

extern "C"
JNIEXPORT jobjectArray JNICALL Java_com_intellij_memory_agent_proxy_IdeaNativeAgentProxy_gcRoots(
        JNIEnv *env,
        jclass thisClass,
        jobject object) {
    return findGcRoots(env, gdata->jvmti, thisClass, object);
}

extern "C"
JNIEXPORT jboolean JNICALL Java_com_intellij_memory_agent_proxy_IdeaNativeAgentProxy_isLoadedImpl(
        JNIEnv *env,
        jclass thisClass) {
    return (uint8_t) 1;
}
