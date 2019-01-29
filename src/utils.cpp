// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#include <jvmti.h>
#include <sstream>
#include <vector>
#include "utils.h"
#include "log.h"

const char *getReferenceTypeDescription(jvmtiHeapReferenceKind kind) {
    if (kind == JVMTI_HEAP_REFERENCE_CLASS) return "Reference from an object to its class.";
    if (kind == JVMTI_HEAP_REFERENCE_FIELD)
        return "Reference from an object to the value of one of its instance fields.";
    if (kind == JVMTI_HEAP_REFERENCE_ARRAY_ELEMENT) return "Reference from an array to one of its elements.";
    if (kind == JVMTI_HEAP_REFERENCE_CLASS_LOADER) return "Reference from a class to its class loader.";
    if (kind == JVMTI_HEAP_REFERENCE_SIGNERS) return "Reference from a class to its signers array.";
    if (kind == JVMTI_HEAP_REFERENCE_PROTECTION_DOMAIN) return "Reference from a class to its protection domain.";
    if (kind == JVMTI_HEAP_REFERENCE_INTERFACE)
        return "Reference from a class to one of its interfaces. Note: interfaces are defined via a constant pool "
               "reference, so the referenced interfaces may also be reported with a JVMTI_HEAP_REFERENCE_CONSTANT_"
               "POOL reference kind.";
    if (kind == JVMTI_HEAP_REFERENCE_STATIC_FIELD)
        return "Reference from a class to the value of one of its static fields.";
    if (kind == JVMTI_HEAP_REFERENCE_CONSTANT_POOL)
        return "Reference from a class to a resolved entry in the constant pool.";
    if (kind == JVMTI_HEAP_REFERENCE_SUPERCLASS)
        return "Reference from a class to its superclass. A callback is bot sent if the superclass is "
               "java.lang.Object. Note: loaded classes define superclasses via a constant pool reference, "
               "so the referenced superclass may also be reported with a JVMTI_HEAP_REFERENCE_CONSTANT_POOL "
               "reference kind.";
    if (kind == JVMTI_HEAP_REFERENCE_JNI_GLOBAL) return "Heap root reference: JNI global reference.";
    if (kind == JVMTI_HEAP_REFERENCE_SYSTEM_CLASS) return "Heap root reference: System class.";
    if (kind == JVMTI_HEAP_REFERENCE_MONITOR) return "Heap root reference: monitor.";
    if (kind == JVMTI_HEAP_REFERENCE_STACK_LOCAL) return "Heap root reference: local variable on the stack.";
    if (kind == JVMTI_HEAP_REFERENCE_JNI_LOCAL) return "Heap root reference: JNI local reference.";
    if (kind == JVMTI_HEAP_REFERENCE_THREAD) return "Heap root reference: Thread.";
    if (kind == JVMTI_HEAP_REFERENCE_OTHER) return "Heap root reference: other heap root reference.";
    return "Unknown reference kind";
}

bool isIgnoredReference(jvmtiHeapReferenceKind kind) {
    return kind == JVMTI_HEAP_REFERENCE_CLASS ||
           kind == JVMTI_HEAP_REFERENCE_CLASS_LOADER ||
           kind == JVMTI_HEAP_REFERENCE_SIGNERS ||
           kind == JVMTI_HEAP_REFERENCE_PROTECTION_DOMAIN ||
           kind == JVMTI_HEAP_REFERENCE_INTERFACE ||
           kind == JVMTI_HEAP_REFERENCE_SUPERCLASS ||
           kind == JVMTI_HEAP_REFERENCE_CONSTANT_POOL;
}

static std::string from_bool(bool value) {
    return std::string(value ? "true" : "false");
}

jobjectArray toJavaArray(JNIEnv *env, std::vector<jobject> &objects) {
    auto count = static_cast<jsize>(objects.size());
    jobjectArray res = env->NewObjectArray(count, env->FindClass("java/lang/Object"), nullptr);
    for (auto i = 0; i < count; ++i) {
        env->SetObjectArrayElement(res, i, objects[i]);
    }
    return res;
}

jlongArray toJavaArray(JNIEnv *env, std::vector<jlong> &items) {
    auto count = static_cast<jsize>(items.size());
    jlongArray result = env->NewLongArray(count);
    env->SetLongArrayRegion(result, 0, count, items.data());
    return result;
}

jintArray toJavaArray(JNIEnv *env, std::vector<jint> &items) {
    auto count = static_cast<jsize>(items.size());
    jintArray result = env->NewIntArray(count);
    env->SetIntArrayRegion(result, 0, count, items.data());
    return result;
}

jintArray toJavaArray(JNIEnv *env, jint value) {
    std::vector<jint> vector = {value};
    return toJavaArray(env, vector);
}

jobjectArray toJavaArray(JNIEnv *env, std::vector<std::vector<jint>> &prev) {
    jobjectArray res = env->NewObjectArray(static_cast<jsize>(prev.size()),
                                           env->FindClass("java/lang/Object"),
                                           nullptr);
    for (int i = 0; i < prev.size(); i++) {
        auto parentСount = static_cast<jsize>(prev[i].size());
        jintArray intArray = env->NewIntArray(parentСount);
        env->SetIntArrayRegion(intArray, 0, parentСount, prev[i].data());
        env->SetObjectArrayElement(res, i, intArray);
    }

    return res;
}

jobjectArray wrapWithArray(JNIEnv *env, jobject first, jobject second) {
    jobjectArray res = env->NewObjectArray(2, env->FindClass("java/lang/Object"), nullptr);
    env->SetObjectArrayElement(res, 0, first);
    env->SetObjectArrayElement(res, 1, second);
    return res;
}

void handleError(jvmtiEnv *jvmti, jvmtiError err, const char *message) {
    if (err != JVMTI_ERROR_NONE) {
        char *errorName = nullptr;
        const char *name;
        if (jvmti->GetErrorName(err, &errorName) != JVMTI_ERROR_NONE) {
            name = "UNKNOWN";
        } else {
            name = errorName;
        }

        std::stringstream ss;

        ss << "ERROR: JVMTI: " << err << "(" << name << "): " << message << std::endl;
        error(ss.str().data());
    }
}

