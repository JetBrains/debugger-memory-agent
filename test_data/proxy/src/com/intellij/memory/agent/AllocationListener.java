package com.intellij.memory.agent;

public interface AllocationListener {
    void onAllocation(Thread thread, Object obj, Class<?> objClass, long size);
}
