package com.intellij.memory.agent.proxy;

public class IdeaNativeAgentProxy {
  public IdeaNativeAgentProxy() {
  }

  public static native Object gcRoots(Object var0);

  public static native long size(Object var0);

  public static boolean isLoaded() {
    try {
      return isLoadedImpl();
    } catch (Throwable t) {
      return false;
    }
  }

  private static native boolean isLoadedImpl();
}
