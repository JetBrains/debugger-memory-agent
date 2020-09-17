// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#include "infos.h"

ReferenceInfo::ReferenceInfo(jlong tag, jvmtiHeapReferenceKind kind) : tag(tag), kind(kind) {

}

InfoWithIndex::InfoWithIndex(jlong tag, jvmtiHeapReferenceKind kind, jint index) : ReferenceInfo(tag, kind), index(index) {

}

StackInfo::StackInfo(jlong tag, jvmtiHeapReferenceKind kind, jlong threadId, jint slot, jint depth, jmethodID methodId) :
        ReferenceInfo(tag, kind), threadId(threadId), slot(slot), depth(depth), methodId(methodId) {

}

static jlongArray buildStackInfo(JNIEnv *env, jlong threadId, jint depth, jint slot) {
    std::vector<jlong> vector = {threadId, depth, slot};
    return toJavaArray(env, vector);
}

static jobjectArray buildMethodInfo(JNIEnv *env, jvmtiEnv *jvmti, jmethodID id) {
    if (id == nullptr) {
        return nullptr;
    }
    char *name, *signature, *genericSignature;
    jvmtiError err = jvmti->GetMethodName(id, &name, &signature, &genericSignature);
    handleError(jvmti, err, "Could not receive method info");
    if (err != JVMTI_ERROR_NONE) return nullptr;
    jobjectArray result = env->NewObjectArray(3, env->FindClass("java/lang/String"), nullptr);
    env->SetObjectArrayElement(result, 0, env->NewStringUTF(name));
    env->SetObjectArrayElement(result, 1, env->NewStringUTF(signature));
    env->SetObjectArrayElement(result, 2, env->NewStringUTF(genericSignature));
    jvmti->Deallocate(reinterpret_cast<unsigned char *>(name));
    jvmti->Deallocate(reinterpret_cast<unsigned char *>(signature));
    jvmti->Deallocate(reinterpret_cast<unsigned char *>(genericSignature));
    return result;
}

jobject StackInfo::getReferenceInfo(JNIEnv *env, jvmtiEnv *jvmti) {
    return wrapWithArray(env,
            buildStackInfo(env, threadId, depth, slot),
            buildMethodInfo(env, jvmti, methodId)
    );
}

ReferenceInfo *createReferenceInfo(jlong tag, jvmtiHeapReferenceKind kind, const jvmtiHeapReferenceInfo *info) {
    switch (kind) {
        case JVMTI_HEAP_REFERENCE_STATIC_FIELD:
        case JVMTI_HEAP_REFERENCE_FIELD:
            return new InfoWithIndex(tag, kind, info->field.index);
        case JVMTI_HEAP_REFERENCE_ARRAY_ELEMENT:
            return new InfoWithIndex(tag, kind, info->array.index);
        case JVMTI_HEAP_REFERENCE_CONSTANT_POOL:
            return new InfoWithIndex(tag, kind, info->constant_pool.index);
        case JVMTI_HEAP_REFERENCE_STACK_LOCAL: {
            jvmtiHeapReferenceInfoStackLocal const &stackLocal = info->stack_local;
            return new StackInfo(tag, kind, stackLocal.thread_id, stackLocal.slot, stackLocal.depth,
                                 stackLocal.method);
        }
        case JVMTI_HEAP_REFERENCE_JNI_LOCAL: {
            jvmtiHeapReferenceInfoJniLocal const &jniLocal = info->jni_local;
            return new StackInfo(tag, kind, jniLocal.thread_id, -1, jniLocal.depth, jniLocal.method);
        }
            // no information provided
        case JVMTI_HEAP_REFERENCE_CLASS:
        case JVMTI_HEAP_REFERENCE_CLASS_LOADER:
        case JVMTI_HEAP_REFERENCE_SIGNERS:
        case JVMTI_HEAP_REFERENCE_PROTECTION_DOMAIN:
        case JVMTI_HEAP_REFERENCE_INTERFACE:
        case JVMTI_HEAP_REFERENCE_SUPERCLASS:
        case JVMTI_HEAP_REFERENCE_JNI_GLOBAL:
        case JVMTI_HEAP_REFERENCE_SYSTEM_CLASS:
        case JVMTI_HEAP_REFERENCE_MONITOR:
        case JVMTI_HEAP_REFERENCE_THREAD:
        case JVMTI_HEAP_REFERENCE_OTHER:
            break;
    }

    return new ReferenceInfo(tag, kind);
}
