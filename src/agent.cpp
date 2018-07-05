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

using namespace std;

static GlobalAgentData *gdata;

JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM *jvm, char *options, void *reserved) {
    jvmtiEnv *jvmti = nullptr;
    jvmtiCapabilities capa;
    jvmtiError error;

    jint result = jvm->GetEnv((void **) &jvmti, JVMTI_VERSION_1_0);
    if (result != JNI_OK) {
        printf("ERROR: Unable to access JVMTI!\n");
    }

    (void) memset(&capa, 0, sizeof(jvmtiCapabilities));
    capa.can_tag_objects = 1;
    error = (jvmti)->AddCapabilities(&capa);

    gdata = (GlobalAgentData *) malloc(sizeof(GlobalAgentData));
    gdata->jvmti = jvmti;
    return JNI_OK;
}

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

