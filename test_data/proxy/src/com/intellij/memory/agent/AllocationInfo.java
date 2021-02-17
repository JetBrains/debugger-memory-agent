package com.intellij.memory.agent;

/**
 * This class holds information that is provided on allocation event.
 *
 * @see #getObject()
 * @see #getThread()
 * @see #getSize()
 * @see #getObjectClass()
 */
public class AllocationInfo {
    private final Thread thread;
    private final Object object;
    private final Class<?> objectClass;
    private final long size;

    public AllocationInfo(Thread thread, Object obj, Class<?> objectClass, long size) {
        this.thread = thread;
        this.object = obj;
        this.objectClass = objectClass;
        this.size = size;
    }

    /**
     * @return Thread where allocation is performed
     */
    public Thread getThread() {
        return thread;
    }

    /**
     * @return Allocated object
     */
    public Object getObject() {
        return object;
    }

    /**
     * @return Allocated object's class
     */
    public Class<?> getObjectClass() {
        return objectClass;
    }

    /**
     * @return Size of allocation in bytes
     */
    public long getSize() {
        return size;
    }
}
