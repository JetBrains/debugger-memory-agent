#ifndef NATIVE_MEMORY_AGENT_UTILS_H
#define NATIVE_MEMORY_AGENT_UTILS_H
#include "types.h"
#include <string>

const char *get_reference_type_description(jvmtiHeapReferenceKind kind);

std::string get_tag_description(Tag* tag);

#endif //NATIVE_MEMORY_AGENT_UTILS_H
