// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#ifndef NATIVE_MEMORY_AGENT_UTILS_H
#define NATIVE_MEMORY_AGENT_UTILS_H

#include "types.h"
#include <string>
#include <set>
#include <jvmti.h>

typedef void (*tagReleasedCallback)(jlong tag);

const char *getReferenceTypeDescription(jvmtiHeapReferenceKind kind);

jobjectArray toJavaArray(JNIEnv *env, std::vector<jobject> &objects);

jintArray toJavaArray(JNIEnv *env, std::vector<jint> &items);

jlongArray toJavaArray(JNIEnv *env, std::vector<jlong> &items);

jintArray toJavaArray(JNIEnv *env, jint value);

jobjectArray wrapWithArray(JNIEnv *env, jobject first, jobject second);

void handleError(jvmtiEnv *jvmti, jvmtiError err, const char *message);

jvmtiError removeAllTagsFromHeap(jvmtiEnv *jvmti, tagReleasedCallback callback);

jvmtiError removeTagsFromHeap(jvmtiEnv *jvmti, std::set<jlong> &ignoredTags, tagReleasedCallback callback);

jvmtiError cleanHeapAndGetObjectsByTags(jvmtiEnv *jvmti, std::vector<jlong> &tags,
                                        std::vector<std::pair<jobject, jlong>> &result,
                                        tagReleasedCallback callback);

#endif //NATIVE_MEMORY_AGENT_UTILS_H
