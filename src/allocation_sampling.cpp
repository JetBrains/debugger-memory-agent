// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#include "allocation_sampling.h"
#include "utils.h"

const char *ArrayOfListeners::arrayOfListenersClassName = "com/intellij/memory/agent/ArrayOfListeners";
const char *ArrayOfListeners::listenerHoldersFieldName = "listenerHolders";
const char *ArrayOfListeners::listenerHoldersFieldSignature = "[Ljava/lang/Object;";
const char *ArrayOfListeners::listenerHolderClassName = "com/intellij/memory/agent/AllocationListenerHolder";
const char *ArrayOfListeners::notificationMethodName = "notifyListenerIfNeeded";
const char *ArrayOfListeners::notificationMethodSignature = "(Ljava/lang/Thread;Ljava/lang/Object;Ljava/lang/Class;J)V";

ArrayOfListeners arrayOfListeners;

extern "C" JNIEXPORT void JNICALL SampledObjectAlloc(jvmtiEnv *jvmti, JNIEnv *env, jthread thread,
                                                     jobject object, jclass klass, jlong size) {
    arrayOfListeners.notifyAll(env, thread, object, klass, size);
}

void ArrayOfListeners::init(JNIEnv *env, jobject array) {
    if (!mirrorObject) {
        jclass arrayOfListenersClass = env->FindClass(arrayOfListenersClassName);
        listenerHoldersField = env->GetFieldID(arrayOfListenersClass, listenerHoldersFieldName, listenerHoldersFieldSignature);
        jclass listenerHolderClass = env->FindClass(listenerHolderClassName);
        notificationMethod = env->GetMethodID(listenerHolderClass, notificationMethodName, notificationMethodSignature);
        mirrorObject = env->NewGlobalRef(array);
    } else {
        env->DeleteGlobalRef(mirrorObject);
        mirrorObject = env->NewGlobalRef(array);
    }
}

void ArrayOfListeners::notifyAll(JNIEnv *env, jthread thread, jobject object, jclass klass, jlong size) const {
    if (!mirrorObject) {
        return;
    }

    auto listenerHolders = reinterpret_cast<jobjectArray>(
            env->GetObjectField(mirrorObject, listenerHoldersField)
    );

    for (jsize i = 0; i < env->GetArrayLength(listenerHolders); i++) {
        jobject listenerHolder = env->GetObjectArrayElement(listenerHolders, i);
        env->CallVoidMethod(listenerHolder, notificationMethod, thread, object, klass, size);
    }
}
