// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#ifndef MEMORY_AGENT_SIZES_CALLBACKS_H
#define MEMORY_AGENT_SIZES_CALLBACKS_H

#include <unordered_set>
#include <chrono>
#include "jni.h"
#include "jvmti.h"

extern std::unordered_set<jlong> tagsWithNewInfo;

jint JNICALL getTagsWithNewInfo     (jvmtiHeapReferenceKind refKind, const jvmtiHeapReferenceInfo *refInfo, jlong classTag,
                                     jlong referrerClassTag, jlong size, jlong *tagPtr,
                                     jlong *referrerTagPtr, jint length, void *userData);

jint JNICALL visitReference         (jvmtiHeapReferenceKind refKind, const jvmtiHeapReferenceInfo *refInfo, jlong classTag,
                                     jlong referrerClassTag, jlong size, jlong *tagPtr,
                                     jlong *referrerTagPtr, jint length, void *userData);

jint JNICALL spreadInfo             (jvmtiHeapReferenceKind refKind, const jvmtiHeapReferenceInfo *refInfo, jlong classTag,
                                     jlong referrerClassTag, jlong size, jlong *tagPtr,
                                     jlong *referrerTagPtr, jint length, void *userData);

jint JNICALL visitObject            (jlong classTag, jlong size, jlong *tagPtr, jint length, void *userData);

jint JNICALL clearTag               (jlong classTag, jlong size, jlong *tagPtr, jint length, void *userData);

jint JNICALL retagStartObjects      (jlong classTag, jlong size, jlong *tagPtr, jint length, void *userData);

jint JNICALL tagObjectOfTaggedClass (jlong classTag, jlong size, jlong *tagPtr, jint length, void *userData);

jvmtiError walkHeapFromObjects      (jvmtiEnv *jvmti,
                                     const std::vector<std::pair<jobject, jlong>> &objectsAndTags,
                                     const std::chrono::steady_clock::time_point &finishTime);

#endif //MEMORY_AGENT_SIZES_CALLBACKS_H
