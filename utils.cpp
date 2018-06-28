#include <jvmti.h>
#include "utils.h"


const char *get_reference_type_description(jvmtiHeapReferenceKind kind) {
    if (kind == JVMTI_HEAP_REFERENCE_ARRAY_ELEMENT) return "array";
    if (kind == JVMTI_HEAP_REFERENCE_FIELD) return "field";
    return "unknown reference";
}

static std::string from_bool(bool value) {
    return std::string(value ? "true" : "false");
}

std::string get_tag_description(Tag *tag) {
    return std::string("tag[start = ") + from_bool(tag->start_object) +
           std::string(", in_subtree = ") + from_bool(tag->in_subtree) +
           std::string(", reachable outside = ") + from_bool(tag->reachable_outside) + std::string("]");

}


