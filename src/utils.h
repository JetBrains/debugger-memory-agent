// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#ifndef NATIVE_MEMORY_AGENT_UTILS_H
#define NATIVE_MEMORY_AGENT_UTILS_H

#include "global_data.h"
#include <string>
#include <set>
#include <functional>
#include <jvmti.h>

class ThreadSuspender {
public:
    explicit ThreadSuspender(jvmtiEnv *jvmti);
    ~ThreadSuspender();

private:
    jvmtiEnv *jvmti;
    std::vector<jthread> suspendedThreads;
};

typedef void (*tagReleasedCallback)(jlong tag);

const char *getReferenceTypeDescription(jvmtiHeapReferenceKind kind);

jbooleanArray toJavaArray(JNIEnv *env, std::vector<jboolean> &items);

jobjectArray toJavaArray(JNIEnv *env, std::vector<jobject> &objects);

jobjectArray toJavaArray(JNIEnv *env, std::vector<jclass> &objects);

jintArray toJavaArray(JNIEnv *env, std::vector<jint> &items);

jlongArray toJavaArray(JNIEnv *env, std::vector<jlong> &items);

jintArray toJavaArray(JNIEnv *env, jint value);

jlongArray toJavaArray(JNIEnv *env, jlong value);

void fromJavaArray(JNIEnv *env, jobjectArray javaArray, std::vector<jobject> &result);

std::vector<jobject> fromJavaArray(JNIEnv *env, jobjectArray javaArray);

jobjectArray wrapWithArray(JNIEnv *env, jobject first, jobject second);

void handleError(jvmtiEnv *jvmti, jvmtiError err, const char *message);

jvmtiError removeAllTagsFromHeap(jvmtiEnv *jvmti, tagReleasedCallback callback);

jvmtiError removeTagsFromHeap(jvmtiEnv *jvmti, std::set<jlong> &ignoredTags, tagReleasedCallback callback);

jvmtiError getObjectsByTags(jvmtiEnv *jvmti, std::vector<jlong> &tags,
                            std::vector<std::pair<jobject, jlong>> &result);

jvmtiError getObjectsByTags(jvmtiEnv *jvmti, std::vector<jlong> &&tags,
                            std::vector<std::pair<jobject, jlong>> &result);

jvmtiError getObjectsByTags(jvmtiEnv *jvmti, std::vector<jlong> &tags,
                            std::vector<jobject> &result);

jvmtiError getObjectsByTags(jvmtiEnv *jvmti, std::vector<jlong> &&tags,
                            std::vector<jobject> &result);

jvmtiError cleanHeapAndGetObjectsByTags(jvmtiEnv *jvmti, std::vector<jlong> &tags,
                                        std::vector<std::pair<jobject, jlong>> &result,
                                        tagReleasedCallback callback);

jvmtiError tagClassAndItsInheritors(JNIEnv *env, jvmtiEnv *jvmti, jobject classObject, std::function<jlong (jlong)> &&createTag);

jvmtiError tagClassAndItsInheritorsSimple(JNIEnv *env, jvmtiEnv *jvmti, jobject classObject);

bool isOk(jvmtiError error);

bool fileExists(const std::string &fileName);

std::string jstringTostring(JNIEnv *env, jstring jStr);

jmethodID getIsAssignableFromMethod(JNIEnv *env);

jobject getClassLoader(JNIEnv *env, jclass obj);

bool isEqual(JNIEnv *env, jobject obj1, jobject obj2);

std::string getToString(JNIEnv *env, jobject klass);

template<typename T>
jlong pointerToTag(T tag) {
    return reinterpret_cast<jlong>(tag);
}

#endif //NATIVE_MEMORY_AGENT_UTILS_H
