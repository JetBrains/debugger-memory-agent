#ifndef NATIVE_MEMORY_AGENT_OBJECT_SIZE_H
#define NATIVE_MEMORY_AGENT_OBJECT_SIZE_H

#include <jvmti.h>

jint estimateObjectSize(JNIEnv *jni, jvmtiEnv *jvmti, jclass thisClass, jobject object);

#endif //NATIVE_MEMORY_AGENT_OBJECT_SIZE_H
