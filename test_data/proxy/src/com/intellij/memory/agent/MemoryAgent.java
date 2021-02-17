package com.intellij.memory.agent;

import java.io.File;
import java.lang.reflect.Array;
import java.util.concurrent.Callable;

/**
 * <p>Memory agent allows to perform heap diagnostics or attach allocation listeners for
 * <a href="https://openjdk.java.net/jeps/331">low-overhead heap profiling</a> using
 * <a href="https://docs.oracle.com/javase/8/docs/technotes/guides/jvmti/">JVMTI</a>.<p/>
 *
 * <p>To access the memory agent's instance use {@link #get()}. If the binary wasn't loaded
 * successfully you can get the corresponding exception by calling {@link #getLoadingException()}.<p/>
 *
 * <p>All methods of this class (except for {@link #get()}) throw {@link com.intellij.memory.agent.MemoryAgentExecutionException}
 * if a call to a native method failed.<p/>
 */
public class MemoryAgent {
    private static final String allocationSamplingIsNotSupportedMessage = "Allocation sampling is not supported for this version of jdk";
    private static final String agentLibraryWasNotLoadedMessage = "Agent library wasn't loaded";
    private static final String failedToCallNativeMethodMessage = "Failed to call native method";

    private final IdeaNativeAgentProxy proxy;
    private ArrayOfListeners listeners = null;

    private static class LazyHolder {
        static final MemoryAgent INSTANCE = new MemoryAgent();
        static Exception loadingException = null;
        static {
            try {
                File agentLib = AgentExtractor.extract(new File(System.getProperty("java.io.tmpdir")));
                System.load(agentLib.getAbsolutePath());
            } catch (Exception ex) {
                loadingException = ex;
            }
        }
    }

    private MemoryAgent() {
        proxy = new IdeaNativeAgentProxy();
    }

    /**
     * Loads memory agent to JVM on the first invocation and returns its instance on success.
     * If an error occurred while loading you can get the corresponding exception by calling
     * {@link #getLoadingException()}.
     *
     * @return null if the agent wasn't loaded successfully, else MemoryAgent's instance.
     */
    public static MemoryAgent get() {
        if (LazyHolder.loadingException != null) {
            return null;
        }
        return LazyHolder.INSTANCE;
    }

    /**
     * @return An exception that was thrown during binary loading,
     * or null if the agent was loaded successfully
     */
    public static Exception getLoadingException() {
        return LazyHolder.loadingException;
    }

    /**
     * Finds the first reachable object of this class and its inheritors in heap.
     *
     * @param startObject Start object, from which the heap is traversed. Pass null if you want to traverse
     *                    the heap from GC roots.
     * @param suspectClass The class of a sought object.
     * @param <T> The type of a sought object.
     * @return An instance of first found reachable object of {@code suspectClass} class in heap or null if none was found.
     * @throws MemoryAgentExecutionException if a call to a native method failed.
     */
    @SuppressWarnings("unchecked")
    public synchronized <T> T getFirstReachableObject(Object startObject, Class<T> suspectClass) throws MemoryAgentExecutionException {
        return (T)getResult(callProxyMethod(() -> proxy.getFirstReachableObject(startObject, suspectClass)));
    }

    /**
     * Finds all reachable objects of this class and its inheritors in heap.
     *
     * @param startObject Start object, from which the heap is traversed. Pass null if you want to traverse
     *                    the heap from GC roots.
     * @param suspectClass The class of sought objects.
     * @param <T> The type of sought objects.
     * @return An array of all reachable objects of {@code suspectClass} class in heap.
     * @throws MemoryAgentExecutionException if a call to a native method failed.
     */
    @SuppressWarnings("unchecked")
    public synchronized <T> T[] getAllReachableObjects(Object startObject, Class<T> suspectClass) throws MemoryAgentExecutionException {
        Object[] foundObjects = (Object [])getResult(callProxyMethod(() -> proxy.getAllReachableObjects(startObject, suspectClass)));
        T[] resultArray = (T[])Array.newInstance(suspectClass, foundObjects.length);
        for (int i = 0; i < foundObjects.length; i++) {
            resultArray[i] = (T)foundObjects[i];
        }
        return resultArray;
    }

    /**
     * Adds an allocation listener that catches allocation sampling events for specified classes.
     *
     * @param allocationListener  {@link AllocationListener} instance to track allocation sampling events.
     * @param trackedClasses An array of classes to track allocations for.
     * @throws MemoryAgentExecutionException if a call to a native method failed.
     *
     * @see #addAllocationListener
     * @see <a href="https://openjdk.java.net/jeps/331">Low-Overhead Heap Profiling</a>
     */
    public synchronized void addAllocationListener(AllocationListener allocationListener, Class<?>... trackedClasses) throws MemoryAgentExecutionException {
        if (listeners == null) {
            listeners = new ArrayOfListeners();
            if (!callProxyMethod(() ->IdeaNativeAgentProxy.initArrayOfListeners(listeners))) {
                throw new MemoryAgentExecutionException(allocationSamplingIsNotSupportedMessage);
            }
        }
        listeners.add(allocationListener, trackedClasses);
    }

    /**
     * Adds an allocation listener that catches all allocation sampling events.
     *
     * <p>
     * In this example stacktrace of each allocation is printed:
     * <pre>
     * {@code
     * MemoryAgent agent = MemoryAgent.get();
     * agent.addAllocationListener((info) -> {
     *     for (StackTraceElement element : info.getThread().getStackTrace()) {
     *         System.out.println(element);
     *     }
     * });
     * }
     * <pre/>
     * </p>
     * @param allocationListener {@link AllocationListener} instance to track allocation sampling events.
     * @throws MemoryAgentExecutionException if a call to a native method failed.
     * @see <a href="https://openjdk.java.net/jeps/331">Low-Overhead Heap Profiling</a>
     */
    public synchronized void addAllocationListener(AllocationListener allocationListener) throws MemoryAgentExecutionException {
        addAllocationListener(allocationListener, new Class[0]);
    }

    /**
     * Removes allocation listener from an internal data structure, so it won't receive
     * any allocation events.
     *
     * @param allocationListener An instance of an allocation listener to remove.
     */
    public synchronized void removeAllocationListener(AllocationListener allocationListener) {
        if (listeners != null) {
            listeners.remove(allocationListener);
        }
    }

    /**
     * Set heap allocation sampling interval.
     *
     * @param interval Sampling interval in bytes.
     * @throws MemoryAgentExecutionException if a call to a native method failed.
     * @see <a href="https://openjdk.java.net/jeps/331">Low-Overhead Heap Profiling</a>
     */
    public void setHeapSamplingInterval(long interval) throws MemoryAgentExecutionException {
        if (!callProxyMethod(() -> IdeaNativeAgentProxy.setHeapSamplingInterval(interval))) {
            throw new MemoryAgentExecutionException(allocationSamplingIsNotSupportedMessage);
        }
    }

    /**
     * Enables allocation sampling events. Allocation sampling is enabled by default
     * if the running JVM supports it.
     *
     * @see <a href="https://openjdk.java.net/jeps/331">Low-Overhead Heap Profiling</a>
     * @throws MemoryAgentExecutionException if a call to a native method failed or
     * allocation sampling is not supported
     */
    public void enableAllocationSampling() throws MemoryAgentExecutionException {
        if (!callProxyMethod(IdeaNativeAgentProxy::enableAllocationSampling)) {
            throw new MemoryAgentExecutionException(allocationSamplingIsNotSupportedMessage);
        }
    }

    /**
     * Disables allocation sampling events.
     *
     * @see <a href="https://openjdk.java.net/jeps/331">Low-Overhead Heap Profiling</a>
     * @throws MemoryAgentExecutionException if a call to a native method failed.
     */
    public void disableAllocationSampling() throws MemoryAgentExecutionException {
        callProxyMethod(IdeaNativeAgentProxy::disableAllocationSampling);
    }

    private static Object getResult(Object result) {
        return ((Object[])result)[1];
    }

    private static <T> T callProxyMethod(Callable<T> callable) throws MemoryAgentExecutionException {
        if (!IdeaNativeAgentProxy.isLoaded()) {
            throw new MemoryAgentExecutionException(agentLibraryWasNotLoadedMessage);
        }

        try {
            return callable.call();
        } catch (Exception ex) {
            throw new MemoryAgentExecutionException(failedToCallNativeMethodMessage, ex);
        }
    }
}
