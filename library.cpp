#include <jvmti.h>
#include <vector>
#include <stdio.h>
#include <iostream>

using namespace std;

typedef struct {
    jvmtiEnv *jvmti;
} GlobalAgentData;

static GlobalAgentData *gdata;

typedef struct Tag {
    bool in_subtree;
    bool reachable_outside;
    bool start_object;
} Tag;

static Tag *pointerToTag(jlong tag_ptr) {
    if (tag_ptr == 0) {
        return new Tag();
    }
    return (Tag *) (ptrdiff_t) (void *) tag_ptr;
}

static jlong tagToPointer(Tag *tag) {
    return (jlong) (ptrdiff_t) (void *) tag;
}

JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM *jvm, char *options, void *reserved) {
    jvmtiEnv *jvmti = NULL;
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
JNIEXPORT jint cbHeapReference(jvmtiHeapReferenceKind reference_kind,
                               const jvmtiHeapReferenceInfo *reference_info, jlong class_tag,
                               jlong referrer_class_tag, jlong size, jlong *tag_ptr,
                               jlong *referrer_tag_ptr, jint length, void *user_data) {
    jvmtiError err;
    if (reference_kind == JVMTI_HEAP_REFERENCE_CONSTANT_POOL)
        return JVMTI_VISIT_OBJECTS;
    if (reference_kind != JVMTI_HEAP_REFERENCE_FIELD
        && reference_kind != JVMTI_HEAP_REFERENCE_ARRAY_ELEMENT
        && reference_kind != JVMTI_HEAP_REFERENCE_STATIC_FIELD) {
        //We won't bother propogating pointers along other kinds of references
        //(e.g. from a class to its classloader - see http://docs.oracle.com/javase/7/docs/platform/jvmti/jvmti.html#jvmtiHeapReferenceKind )
        return JVMTI_VISIT_OBJECTS;
    }

    if (reference_kind == JVMTI_HEAP_REFERENCE_STATIC_FIELD) {
        //We are visiting a static field directly.
    }

    //Not directly pointed to by an SF.
    if (*referrer_tag_ptr != 0) {
//        printf("from visited earlier\n");
        Tag *referrer_tag = pointerToTag(*referrer_tag_ptr);
        if (*tag_ptr != 0) {
//            printf("to visited earlier\n");
            Tag *referee_tag = pointerToTag(*tag_ptr);
            if (referrer_tag->in_subtree) {
                if (!referee_tag->in_subtree) {
                    referee_tag->in_subtree = true;
                    ((std::vector<jlong> *) (ptrdiff_t) user_data)->push_back(*tag_ptr);
                }
            } else {
                referee_tag->reachable_outside = true;
            }
        } else {
//            printf("to not visited earlier\n");
            Tag *referee_tag = new Tag();
            referee_tag->start_object = false;
            referee_tag->reachable_outside = referrer_tag->reachable_outside;
            referee_tag->in_subtree = referrer_tag->in_subtree;
            *tag_ptr = tagToPointer(referee_tag);
        }
    } else {
        // object from root set
//        printf("from root set\n");
        Tag *referrer_tag = new Tag();
        referrer_tag->in_subtree = false;
        referrer_tag->reachable_outside = true;
        referrer_tag->start_object = false;
        *referrer_tag_ptr = tagToPointer(referrer_tag);

        if (*tag_ptr == 0) {
            Tag *referee_tag = new Tag();
            referee_tag->start_object = false;
            referee_tag->reachable_outside = referrer_tag->reachable_outside;
            referee_tag->in_subtree = referrer_tag->in_subtree;
            *tag_ptr = tagToPointer(referee_tag);
        }
    }

    return JVMTI_VISIT_OBJECTS;
}


extern "C"
JNIEXPORT jobject
JNICALL Java_memory_agent_IdeaDebuggerNativeAgentClass_gcRoots(JNIEnv *env, jclass thisClass, jobject object) {
    cout << "HELLLO WORLD" << endl;
    return object;
}

extern "C"
JNIEXPORT jint
JNICALL Java_memory_agent_IdeaDebuggerNativeAgentClass_size(JNIEnv *env, jclass thisClass, jobject object) {
    vector<jlong> tags;
    jvmtiHeapCallbacks *cb = new jvmtiHeapCallbacks();
    memset(cb, 0, sizeof(jvmtiHeapCallbacks));
    cb->heap_reference_callback = &cbHeapReference;

    Tag *tag = new Tag();
    tag->in_subtree = true;
    tag->reachable_outside = false;
    tag->start_object = true;
    tags.push_back(tagToPointer(tag));
    gdata->jvmti->SetTag(object, tagToPointer(tag));
    jint count = 0;
    gdata->jvmti->FollowReferences(JVMTI_HEAP_OBJECT_EITHER, nullptr, nullptr, cb, &tags);
    jobject *objects;
    jlong *objects_tags;
    gdata->jvmti->GetObjectsWithTags(static_cast<jint>(tags.size()), tags.data(), &count, &objects, &objects_tags);

    jint retainedSize = 0;
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
    cout << "Total object retained: " << retainedObjectsCount << endl;
    cout << "Retained size: " << retainedSize << endl;

    return retainedSize;
}