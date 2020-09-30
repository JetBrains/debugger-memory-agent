// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#ifndef MEMORY_AGENT_INFOS_H
#define MEMORY_AGENT_INFOS_H

#include "jni.h"
#include "jvmti.h"
#include "../utils.h"

class ReferenceInfo {
public:
    ReferenceInfo(jlong tag, jvmtiHeapReferenceKind kind);

    virtual jobject getReferenceInfo(JNIEnv *env, jvmtiEnv *jvmti) { return nullptr; }

    jlong getTag() const { return tag; }

    jvmtiHeapReferenceKind getKind() { return kind; }

private:
    jlong tag;
    jvmtiHeapReferenceKind kind;
};

class InfoWithIndex : public ReferenceInfo {
public:
    InfoWithIndex(jlong tag, jvmtiHeapReferenceKind kind, jint index);

    jobject getReferenceInfo(JNIEnv *env, jvmtiEnv *jvmti) override { return toJavaArray(env, index); }

private:
    jint index;
};

class StackInfo : public ReferenceInfo {
public:
    StackInfo(jlong tag, jvmtiHeapReferenceKind kind, jlong threadId, jint slot, jint depth, jmethodID methodId);

public:
    jobject getReferenceInfo(JNIEnv *env, jvmtiEnv *jvmti) override;

private:
    jlong threadId;
    jint slot;
    jint depth;
    jmethodID methodId;
};

ReferenceInfo *createReferenceInfo(jlong tag, jvmtiHeapReferenceKind kind, const jvmtiHeapReferenceInfo *info);

#endif //MEMORY_AGENT_INFOS_H
