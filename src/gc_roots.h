//
// Created by Vitaliy.Bibaev on 05-Jul-18.
//

#ifndef NATIVE_MEMORY_AGENT_GC_ROOTS_H
#define NATIVE_MEMORY_AGENT_GC_ROOTS_H

#include <jni.h>

jobjectArray findGcRoots(JNIEnv *jni, jvmtiEnv *jvmti, jclass thisClass, jobject object);

#endif //NATIVE_MEMORY_AGENT_GC_ROOTS_H
