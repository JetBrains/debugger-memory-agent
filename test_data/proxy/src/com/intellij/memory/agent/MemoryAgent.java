package com.intellij.memory.agent;

import java.io.File;
import java.lang.reflect.Array;
import java.util.LinkedList;
import java.util.List;
import java.util.concurrent.Callable;

public class MemoryAgent {
    private final IdeaNativeAgentProxy proxy;
    private final List<AllocationListenerInfo> listeners;
    private final String allocationSamplingIsNotSupportedMessage = "Allocation sampling is not supported for this version of jdk";

    private static class AllocationListenerInfo {
        public final AllocationListener listener;
        public final int index;

        AllocationListenerInfo(AllocationListener listener, int index) {
            this.listener = listener;
            this.index = index;
        }
    }

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
        listeners = new LinkedList<>();
    }

    public static MemoryAgent get() throws MemoryAgentException {
        Exception ex = LazyHolder.loadingException;
        if (ex != null) {
            throw new MemoryAgentException("Couldn't load memory agent", ex);
        }

        return LazyHolder.INSTANCE;
    }

    @SuppressWarnings("unchecked")
    public <T> T getFirstReachableObject(Object startObject, Class<T> suspectClass) throws MemoryAgentException {
        return (T)getResult(callProxyMethod(() -> proxy.getFirstReachableObject(startObject, suspectClass)));
    }

    @SuppressWarnings("unchecked")
    public <T> T[] getAllReachableObjects(Object startObject, Class<T> suspectClass) throws MemoryAgentException {
        Object[] foundObjects = (Object [])getResult(callProxyMethod(() -> proxy.getAllReachableObjects(startObject, suspectClass)));
        T[] resultArray = (T[])Array.newInstance(suspectClass, foundObjects.length);
        for (int i = 0; i < foundObjects.length; i++) {
            resultArray[i] = (T)foundObjects[i];
        }
        return resultArray;
    }

    public void addAllocationListener(AllocationListener allocationListener, Class<?>... trackedClasses) throws MemoryAgentException {
        int index = callProxyMethod(() -> IdeaNativeAgentProxy.addAllocationListener(allocationListener, trackedClasses));
        if (index < 0) {
            throw new MemoryAgentException(allocationSamplingIsNotSupportedMessage);
        }

        listeners.add(new AllocationListenerInfo(allocationListener, index));
    }

    public void addAllocationListener(AllocationListener allocationListener) throws MemoryAgentException {
        addAllocationListener(allocationListener, new Class[0]);
    }

    public void removeAllocationListener(AllocationListener allocationListener) throws MemoryAgentException {
        int index = findListener(allocationListener);
        if (index < 0) {
            return;
        }

        callProxyMethod(() -> IdeaNativeAgentProxy.removeAllocationListener(index));
        listeners.remove(index);
    }

    public void setHeapSamplingInterval(long interval) throws MemoryAgentException {
        if (!callProxyMethod(() ->IdeaNativeAgentProxy.setHeapSamplingInterval(interval))) {
            throw new MemoryAgentException(allocationSamplingIsNotSupportedMessage);
        }
    }

    private int findListener(AllocationListener allocationListener) {
        for (int i = 0; i < listeners.size(); i++) {
            if (listeners.get(i).listener.equals(allocationListener)) {
                return i;
            }
        }

        return -1;
    }

    private static Object getResult(Object result) {
        return ((Object[])result)[1];
    }

    private static <T> T callProxyMethod(Callable<T> callable) throws MemoryAgentException {
        if (!IdeaNativeAgentProxy.isLoaded()) {
            throw new MemoryAgentException("Agent library wasn't loaded");
        }

        try {
            return callable.call();
        } catch (Exception ex) {
            throw new MemoryAgentException("Failed to call native method", ex);
        }
    }
}
