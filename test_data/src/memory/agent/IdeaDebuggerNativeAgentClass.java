package memory.agent;

public class IdeaDebuggerNativeAgentClass {
  public IdeaDebuggerNativeAgentClass() {
  }

  public static native Object gcRoots(Object var0);

  public static native long size(Object var0);

  public static native boolean isLoadedImpl();

  public static boolean isLoaded() {
    try {
      return isLoadedImpl();
    } catch (Throwable t) {
      return false;
    }
  }
}
