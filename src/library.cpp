#include <jvmti.h>
#include <vector>
#include <queue>
#include <cstdio>
#include <iostream>
#include <memory.h>
#include "utils.h"
#include "types.h"
#include <set>
#include <unordered_map>

using namespace std;

static GlobalAgentData *gdata;

int tag_balance_for_sizes = 0;

static Tag *pointerToTag(jlong tag_ptr) {
    if (tag_ptr == 0) {
        ++tag_balance_for_sizes;
        return new Tag();
    }
    return (Tag *) (ptrdiff_t) (void *) tag_ptr;
}

static jlong tagToPointer(Tag *tag) {
    return (jlong) (ptrdiff_t) (void *) tag;
}

static Tag *createTag(bool start, bool in_subtree, bool reachable_outside) {
    auto *tag = new Tag();
    ++tag_balance_for_sizes;
    tag->in_subtree = in_subtree;
    tag->start_object = start;
    tag->reachable_outside = reachable_outside;
    return tag;
}

JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM *jvm, char *options, void *reserved) {
    jvmtiEnv *jvmti = nullptr;
    jvmtiCapabilities capa;
    jvmtiError error;

    jint result = jvm->GetEnv((void **) &jvmti, JVMTI_VERSION_1_2);
    if (result != JNI_OK) {
        printf("ERROR: Unable to access JVMTI!\n");
    }

    (void) memset(&capa, 0, sizeof(jvmtiCapabilities));
    capa.can_tag_objects = 1;
    error = (jvmti)->AddCapabilities(&capa);

    gdata = (GlobalAgentData *) malloc(sizeof(GlobalAgentData));
    gdata->jvmti = jvmti;
    return JNI_OK;
}

extern "C"
JNIEXPORT
jvmtiIterationControl cbHeapCleanupSizeTags(jlong class_tag, jlong size, jlong *tag_ptr, void *user_data) {
    if (*tag_ptr != 0) {
        Tag *t = pointerToTag(*tag_ptr);
        *tag_ptr = 0;
        delete t;
        --tag_balance_for_sizes;
    }

    return JVMTI_ITERATION_CONTINUE;
}

extern "C"
JNIEXPORT
jvmtiIterationControl cbHeapCleanupGcPaths(jlong class_tag, jlong size, jlong *tag_ptr, void *user_data) {
    if (*tag_ptr != 0) {
        GcTag *t = pointerToGcTag(*tag_ptr);
        *tag_ptr = 0;
        delete t;
    }

    return JVMTI_ITERATION_CONTINUE;
}

static bool is_ignored_reference(jvmtiHeapReferenceKind kind) {
    return kind == JVMTI_HEAP_REFERENCE_CLASS ||
           kind == JVMTI_HEAP_REFERENCE_CLASS_LOADER ||
           kind == JVMTI_HEAP_REFERENCE_SIGNERS ||
           kind == JVMTI_HEAP_REFERENCE_PROTECTION_DOMAIN ||
           kind == JVMTI_HEAP_REFERENCE_INTERFACE ||
           kind == JVMTI_HEAP_REFERENCE_SUPERCLASS ||
           kind == JVMTI_HEAP_REFERENCE_CONSTANT_POOL;
}

extern "C"
JNIEXPORT jint cbHeapReference(jvmtiHeapReferenceKind reference_kind,
                               const jvmtiHeapReferenceInfo *reference_info, jlong class_tag,
                               jlong referrer_class_tag, jlong size, jlong *tag_ptr,
                               jlong *referrer_tag_ptr, jint length, void *user_data) {
    if (is_ignored_reference(reference_kind)) {
        return JVMTI_VISIT_OBJECTS;
    }

    if (referrer_tag_ptr == nullptr) {
        if (tag_ptr == nullptr) {
            cerr << "Unexpected null pointer to referrer and referee tags. Reference kind: "
                 << get_reference_type_description(reference_kind) << endl;
            return JVMTI_VISIT_ABORT;
        }

        if (*tag_ptr != 0) {
            Tag *tag = pointerToTag(*tag_ptr);
            if (!tag->start_object) {
                tag->reachable_outside = true;
            }
        } else {
            *tag_ptr = tagToPointer(createTag(false, false, true));
        }
        return JVMTI_VISIT_OBJECTS;
    }

    if (*referrer_tag_ptr != 0) {
        //referrer has tag
        Tag *referrer_tag = pointerToTag(*referrer_tag_ptr);
        if (referrer_tag->in_subtree) {
            cout << get_tag_description(referrer_tag) << "" << get_reference_type_description(reference_kind)
                 << " link from initial object subtree" << endl;
            cout << *tag_ptr << endl;
        }
        if (*tag_ptr != 0) {
            Tag *referee_tag = pointerToTag(*tag_ptr);
            if (referrer_tag->in_subtree) {
                referee_tag->in_subtree = true;
                ((std::vector<jlong> *) (ptrdiff_t) user_data)->push_back(*tag_ptr);
            }
            if (referrer_tag->reachable_outside && !referee_tag->start_object) {
                referee_tag->reachable_outside = true;
            }
        } else {
            Tag *referee_tag = createTag(false, referrer_tag->in_subtree, referrer_tag->reachable_outside);
            *tag_ptr = tagToPointer(referee_tag);
            if (referee_tag->in_subtree) ((std::vector<jlong> *) (ptrdiff_t) user_data)->push_back(*tag_ptr);
        }
    } else {
        // referrer has no tag yet
        Tag *referrer_tag = createTag(false, false, true);
        if (*tag_ptr != 0) {
            Tag *referee_tag = pointerToTag(*tag_ptr);
            if (!referee_tag->start_object) {
                referee_tag->reachable_outside = true;
            }
        } else {
            Tag *referee_tag = createTag(false, false, true);
            *tag_ptr = tagToPointer(referee_tag);
        }

        *referrer_tag_ptr = tagToPointer(referrer_tag);
    }

    return JVMTI_VISIT_OBJECTS;
}


static void cleanHeapForSizes() {
    // For some reason this call does not iterate through all objects :( Please, do not use it
//    gdata->jvmti->IterateOverReachableObjects(NULL, NULL, &cbHeapCleanupSizeTags, NULL);
    gdata->jvmti->IterateOverHeap(JVMTI_HEAP_OBJECT_TAGGED, &cbHeapCleanupSizeTags, nullptr);
}

static void cleanHeapForGcRoots() {
    gdata->jvmti->IterateOverHeap(JVMTI_HEAP_OBJECT_TAGGED, &cbHeapCleanupGcPaths, nullptr);
}

// TODO: Return jlong
extern "C"
JNIEXPORT jint
JNICALL Java_memory_agent_IdeaDebuggerNativeAgentClass_size(JNIEnv *env, jclass thisClass, jobject object) {
    vector<jlong> tags;
    auto *cb = new jvmtiHeapCallbacks();
    memset(cb, 0, sizeof(jvmtiHeapCallbacks));
    cb->heap_reference_callback = &cbHeapReference;

    Tag *tag = createTag(true, true, false);
    tags.push_back(tagToPointer(tag));
    gdata->jvmti->SetTag(object, tagToPointer(tag));
    jint count = 0;
    gdata->jvmti->FollowReferences(JVMTI_HEAP_OBJECT_EITHER, nullptr, nullptr, cb, &tags);
    jobject *objects;
    jlong *objects_tags;
    gdata->jvmti->GetObjectsWithTags(static_cast<jint>(tags.size()), tags.data(), &count, &objects, &objects_tags);

    jlong retainedSize = 0;
    jint objectsInSubtree = 0;
    jint retainedObjectsCount = 0;
    for (int i = 0; i < count; i++) {
        jlong size;
        jobject obj = objects[i];
        objectsInSubtree++;
        if (!pointerToTag(objects_tags[i])->reachable_outside) {
            gdata->jvmti->GetObjectSize(obj, &size);
            retainedObjectsCount++;
            retainedSize += size;
        }
    }

    cout << "Total objects in the subtree: " << objectsInSubtree << endl;
    cout << "Total objects retained: " << retainedObjectsCount << endl;
    cout << "Retained size: " << retainedSize << endl;

    cleanHeapForSizes();

    if (tag_balance_for_sizes != 0) {
        cerr << "tag balance is not zero: " << tag_balance_for_sizes << endl;
    }
    return static_cast<jint>(retainedSize);
}

extern "C"
JNIEXPORT jint cbGcPaths(jvmtiHeapReferenceKind reference_kind,
                         const jvmtiHeapReferenceInfo *reference_info, jlong class_tag,
                         jlong referrer_class_tag, jlong size, jlong *tag_ptr,
                         jlong *referrer_tag_ptr, jint length, void *user_data) {

    if (is_ignored_reference(reference_kind)) {
        return JVMTI_VISIT_OBJECTS;
    }

    if (referrer_tag_ptr == nullptr) {
        if (*tag_ptr == 0) {
            *tag_ptr = gcTagToPointer(createGcTag());
        }
    } else {
        if (*referrer_tag_ptr == 0) {
            *referrer_tag_ptr = gcTagToPointer(createGcTag());
        }

        GcTag *tag = pointerToGcTag(*tag_ptr);
        tag->prev.push_back(*referrer_tag_ptr);
    }
    return JVMTI_VISIT_OBJECTS;
}

static void walk(jlong current, set<jlong> &visited) {
    visited.insert(current);
    if (visited.find(current) != visited.end()) {
        for (jlong tag : pointerToGcTag(current)->prev) {
            walk(tag, visited);
        }
    }
}

static jobjectArray createJavaArrayWithObjectsByTags(JNIEnv *env,
                                                     vector<jlong> &tags,
                                                     unordered_map<jlong, jint> &tag_to_index) {
    jint count;
    jobject *objects;
    jlong *result_tags;
    // TODO: add error handling
    // Note: order of objects in the result of GetObjectsWithTags may differ from tags
    gdata->jvmti->GetObjectsWithTags(static_cast<jint >(tags.size()), tags.data(), &count, &objects, &result_tags);
    if (count != tags.size()) {
        cerr << "could not find all objects by their tags. Found: " << count << ". Expected: " << tags.size() << endl;
        return nullptr;
    }
    // TODO: Reallocate memory
    vector<jobject> object_in_correct_order(static_cast<const unsigned __int64>(count));
    for (int i = 0; i < count; i++) {
        jint object_order = tag_to_index[result_tags[i]];
        object_in_correct_order[object_order] = objects[i];
    }

    return toJavaArray(env, object_in_correct_order.data(), count);
}

extern "C"
JNIEXPORT jobjectArray
JNICALL Java_memory_agent_IdeaDebuggerNativeAgentClass_gcRoots(JNIEnv *env, jclass thisClass, jobject object) {
    auto *cb = new jvmtiHeapCallbacks();
    memset(cb, 0, sizeof(jvmtiHeapCallbacks));
    cb->heap_reference_callback = &cbGcPaths;

    GcTag *tag = createGcTag();
    jvmtiError err = gdata->jvmti->SetTag(object, gcTagToPointer(tag));
    cout << gcTagToPointer(tag) << endl;
    if (err != JVMTI_ERROR_NONE) {
        cerr << "could not set tag to object" << endl;
    }
    gdata->jvmti->FollowReferences(JVMTI_HEAP_OBJECT_EITHER, nullptr, nullptr, cb, nullptr);

    set<jlong> unique_tags;
    walk(gcTagToPointer(tag), unique_tags);
    cout << "Total reachable unique tags: " << unique_tags.size() << endl;

    unordered_map<jlong, jint> tag_to_index;
    vector<jlong> tags;
    jint i = 0;
    for (jlong reachable_tag: unique_tags) {
        tags.push_back(reachable_tag);
        tag_to_index[reachable_tag] = i;
        ++i;
    }

    cout << "start building reversed links" << endl;
    vector<vector<jint>> previous_links(unique_tags.size());
    for (jlong reachable_tag: unique_tags) {
        jint index = tag_to_index[reachable_tag];
        for (jlong prev_tag: pointerToGcTag(reachable_tag)->prev) {
            jint previous_index = tag_to_index[prev_tag];
            previous_links[index].push_back(previous_index);
        }
    }

    cout << "prepare result arrays" << endl;

    jobjectArray objects = createJavaArrayWithObjectsByTags(env, tags, tag_to_index);
    jobjectArray prev = toJavaArray(env, previous_links);
    jobjectArray result = wrapWithArray(env, objects, prev);

    cleanHeapForGcRoots();
    return result;
}

