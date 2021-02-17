// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#ifndef MEMORY_AGENT_ALLOCATION_SAMPLING_H
#define MEMORY_AGENT_ALLOCATION_SAMPLING_H

#include <list>
#include <vector>

#include "jni.h"
#include "jvmti.h"

class ArrayOfListeners {
private:
    static const char *listenerHolderClassName;
    static const char *notificationMethodName;
    static const char *notificationMethodSignature;
    static const char *arrayOfListenersClassName;
    static const char *listenerHoldersFieldName;
    static const char *listenerHoldersFieldSignature;

public:
    void init(JNIEnv *env, jobject array);
    void notifyAll(JNIEnv *env, jthread thread, jobject object, jclass klass, jlong size) const;

private:
    jobject mirrorObject = nullptr;
    jmethodID notificationMethod = nullptr;
    jfieldID listenerHoldersField = nullptr;
};

extern ArrayOfListeners arrayOfListeners;

extern "C" JNIEXPORT void JNICALL SampledObjectAlloc(jvmtiEnv *jvmti, JNIEnv *env, jthread thread,
                                                     jobject object, jclass klass, jlong size);


#endif //MEMORY_AGENT_ALLOCATION_SAMPLING_H
