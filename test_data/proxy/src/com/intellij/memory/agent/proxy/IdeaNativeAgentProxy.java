package com.intellij.memory.agent.proxy;

public class IdeaNativeAgentProxy {
  private IdeaNativeAgentProxy() {
  }

  public static native boolean canEstimateObjectSize();

  public static native boolean canGetRetainedSizeByClasses();

  public static native boolean canGetShallowSizeByClasses();

  public static native boolean canEstimateObjectsSizes();

  public static native boolean canFindPathsToClosestGcRoots();

  public static native Object[] getShallowSizeByClasses(Object[] classes, long timeoutInMillis, Object cancellationFileName);

  public static native Object[] getRetainedSizeByClasses(Object[] classes, long timeoutInMillis, Object cancellationFileName);

  public static native Object[] getShallowAndRetainedSizeByClasses(Object[] classes, long timeoutInMillis, Object cancellationFileName);

  public static native Object[] findPathsToClosestGcRoots(Object object, int pathsNumber, int objectsNumber, long timeoutInMillis, Object cancellationFileName);

  public static native Object[] size(Object object, long timeoutInMillis, Object cancellationFileName);

  public static native Object[] estimateRetainedSize(Object[] objects, long timeoutInMillis, Object cancellationFileName);

  public static native Object[] getFirstReachableObject(Object startObject, Object suspectClass, long timeoutInMillis, Object cancellationFileName);

  public static native Object[] getAllReachableObjects(Object startObject, Object suspectClass, long timeoutInMillis, Object cancellationFileName);

  public static boolean isLoaded() {
    try {
      return isLoadedImpl();
    } catch (Throwable t) {
      return false;
    }
  }

  private static native boolean isLoadedImpl();
}
