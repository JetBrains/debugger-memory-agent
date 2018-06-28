#include <jvmti.h>
#include "utils.h"
#include <vector>

const char *get_reference_type_description(jvmtiHeapReferenceKind kind) {
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

static std::string from_bool(bool value) {
    return std::string(value ? "true" : "false");
}

std::string get_tag_description(Tag *tag) {
    return std::string("tag[start = ") + from_bool(tag->start_object) +
           std::string(", in_subtree = ") + from_bool(tag->in_subtree) +
           std::string(", reachable outside = ") + from_bool(tag->reachable_outside) + std::string("]");

}

PathNodeTag *createTag(bool target, int index) {
    auto *tag = new PathNodeTag();
    tag->target = target;
    tag->index = index;
    tag->prev = new std::vector<PathNodeTag*>();
    return tag;
}

PathNodeTag *createTag(int index) {
    return createTag(false, index);
}

jobject getObjectByTag(GlobalAgentData *gdata, jlong *tag_ptr) {
    jint count = 0;
    jobject *object;
    jlong *objects_tags;
    gdata->jvmti->GetObjectsWithTags(
            static_cast<jint>(1), tag_ptr, &count, &object, &objects_tags);
    return *object;
}

jobjectArray toJArray(JNIEnv *env, std::vector<jobject> *objs) {
    jobjectArray res = env->NewObjectArray(objs->size(), env->FindClass("java/lang/Object"), 0);
    for (auto i = 0; i < objs->size(); ++i) {
        env->SetObjectArrayElement(res, i, (*objs)[i]);
    }
    return res;
}

jintArray toJArray(JNIEnv *env, std::vector<int> *objs) {
    jintArray res = env->NewIntArray(objs->size());
    env->SetIntArrayRegion(res, 0, objs->size(), objs->data());
    return res;
}

jobjectArray toJArray(JNIEnv *env, std::vector<std::vector<int>*> *objs) {
    jobjectArray res = env->NewObjectArray(objs->size(), env->FindClass("[I"), 0);
    for (auto i = 0; i < objs->size(); ++i) {
        env->SetObjectArrayElement(res, i, toJArray(env, objs->at(i)));
    }
    return res;
}

jobjectArray toJArray(JNIEnv *env, std::vector<jobject> *objs, std::vector<std::vector<int>*> *prev) {
    jobjectArray res = env->NewObjectArray(2, env->FindClass("java/lang/Object"), 0);
    env->SetObjectArrayElement(res, 0, toJArray(env, objs));
    env->SetObjectArrayElement(res, 1, toJArray(env, prev));
    return res;
}

jlong tagToPointer(PathNodeTag *tag) {
    return (jlong) (ptrdiff_t) (void *) tag;
}

PathNodeTag *pointerToPathNodeTag(jlong tag_ptr) {
    if (tag_ptr == 0) {
        return new PathNodeTag();
    }
    return (PathNodeTag *) (ptrdiff_t) (void *) tag_ptr;
}

