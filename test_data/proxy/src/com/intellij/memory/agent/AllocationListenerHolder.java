package com.intellij.memory.agent;

class AllocationListenerHolder {
    AllocationListener listener;
    Class<?>[] trackedClasses;

    AllocationListenerHolder(AllocationListener listener, Class<?>[] trackedClasses) {
        this.listener = listener;
        this.trackedClasses = trackedClasses;
    }

    private void notifyListenerIfNeeded(Thread thread, Object obj, Class<?> objClass, long size) {
        if (trackedClasses.length == 0) {
            notifyListener(thread, obj, objClass, size);
            return;
        }

        for (Class<?> clazz : trackedClasses) {
            if (obj.getClass().equals(clazz)) {
                notifyListener(thread, obj, objClass, size);
                break;
            }
        }
    }

    private void notifyListener(Thread thread, Object obj, Class<?> objClass, long size) {
        listener.onAllocation(new AllocationInfo(thread, obj, objClass, size));
    }
}
