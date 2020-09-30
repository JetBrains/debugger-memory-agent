// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#include "roots_tags.h"

GcTag::GcTag() : state() {
}

GcTag::GcTag(bool isWeakSoftReachable) : state(false, isWeakSoftReachable) {

}

GcTag::~GcTag() {
    --rootsTagBalance;
    for (auto info : backRefs) {
        delete info;
    }
}

GcTag * GcTag::create(jlong classTag) {
    ++rootsTagBalance;
    return new GcTag(classTag != 0 && pointerToGcTag(classTag)->isWeakSoftReachable());
}

GcTag * GcTag::create() {
    ++rootsTagBalance;
    return new GcTag();
}

GcTag *GcTag::pointerToGcTag(jlong tagPtr) {
    if (tagPtr == 0) {
        return new GcTag();
    }
    return (GcTag *) (ptrdiff_t) (void *) tagPtr;
}

void GcTag::cleanTag(jlong tag) {
    delete pointerToGcTag(tag);
}

bool GcTag::isWeakSoftReachable() const {
    return state.isWeakSoftReachable();
}

void GcTag::setVisited() {
    state.setAlreadyVisited(true);
}

void GcTag::updateState(const GcTag *const referrer) {
    state.updateWeakSoftReachableValue(referrer->state);
}

WeakSoftReferenceClassTag::WeakSoftReferenceClassTag() : GcTag(true) {

}

void WeakSoftReferenceClassTag::updateState(const GcTag *const referrer) {

}

GcTag *WeakSoftReferenceClassTag::create() {
    ++rootsTagBalance;
    return new WeakSoftReferenceClassTag();
}
