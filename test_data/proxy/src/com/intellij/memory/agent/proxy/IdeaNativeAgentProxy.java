package com.intellij.memory.agent.proxy;

public class IdeaNativeAgentProxy {
  public String cancellationFileName;
  public long timeoutInMillis;

  public IdeaNativeAgentProxy() {
    cancellationFileName = "";
    timeoutInMillis = -1;
  }

  public IdeaNativeAgentProxy(Object cancellationFileName, long timeoutInMillis) {
    this.cancellationFileName = (String)cancellationFileName;
    this.timeoutInMillis = timeoutInMillis;
  }

  public native boolean canEstimateObjectSize();

  public native boolean canGetRetainedSizeByClasses();

  public native boolean canGetShallowSizeByClasses();

  public native boolean canEstimateObjectsSizes();

  public native boolean canFindPathsToClosestGcRoots();

  public native Object[] getShallowSizeByClasses(Object[] classes);

  public native Object[] getRetainedSizeByClasses(Object[] classes);

  public native Object[] getShallowAndRetainedSizeByClasses(Object[] classes);

  public native Object[] findPathsToClosestGcRoots(Object object, int pathsNumber, int objectsNumber);

  public native Object[] size(Object object);

  public native Object[] estimateRetainedSize(Object[] objects);

  public native Object[] getFirstReachableObject(Object startObject, Object suspectClass);

  public native Object[] getAllReachableObjects(Object startObject, Object suspectClass);

  public static native boolean addAllocationListener(Object allocationListener);

  public static native boolean setHeapSamplingInterval(long interval);

  public static native void clearListeners();

  public boolean isLoaded() {
    try {
      return isLoadedImpl();
    } catch (Throwable t) {
      return false;
    }
  }

  private native boolean isLoadedImpl();
}
