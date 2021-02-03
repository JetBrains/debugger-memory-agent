package com.intellij.memory.agent;

/**
 * An interface that provides capability to catch JVMTI allocation sampling events.
 *
 * @see <a href="https://openjdk.java.net/jeps/331">Low-Overhead Heap Profiling</a>
 */
public interface AllocationListener {
    void onAllocation(AllocationInfo info);
}
