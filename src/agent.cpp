#include <jvmti.h>
#include <vector>
#include <queue>
#include <cstdio>
#include <iostream>
#include <memory.h>
#include <set>
#include <unordered_map>
#include "types.h"
#include "utils.h"
#include "object_size.h"
#include "gc_roots.h"

#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"

using namespace std;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"

static GlobalAgentData *gdata;

JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM *jvm, char *options, void *reserved) {
    jvmtiEnv *jvmti = nullptr;
    jvmtiCapabilities capabilities;
    jvmtiError error;

    jint result = jvm->GetEnv((void **) &jvmti, JVMTI_VERSION_1_0);
    if (result != JNI_OK || jvmti == nullptr) {
        cerr << "ERROR: Unable to access JVMTI!" << std::endl;
        return result;
    }

    (void) memset(&capabilities, 0, sizeof(jvmtiCapabilities));
    capabilities.can_tag_objects = 1;
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

// TODO: Return jlong
extern "C"
JNIEXPORT jint JNICALL Java_memory_agent_IdeaDebuggerNativeAgentClass_size(
        JNIEnv *env,
        jclass thisClass,
        jobject object) {
    return estimateObjectSize(env, gdata->jvmti, thisClass, object);
}

extern "C"
JNIEXPORT jobjectArray JNICALL Java_memory_agent_IdeaDebuggerNativeAgentClass_gcRoots(
        JNIEnv *env,
        jclass thisClass,
        jobject object) {
    return findGcRoots(env, gdata->jvmti, thisClass, object);
}

extern "C"
JNIEXPORT jboolean JNICALL Java_memory_agent_IdeaDebuggerNativeAgentClass_isLoadedImpl(
        JNIEnv *env,
        jclass thisClass) {
    return (uint8_t) 1;
}
