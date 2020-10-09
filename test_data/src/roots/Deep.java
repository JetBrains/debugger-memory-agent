package roots;

import com.intellij.memory.agent.proxy.IdeaNativeAgentProxy;
import common.TestBase;

public class Deep extends TestBase {
  private static final int PATH_LENGTH = 500;

  public static void main(String[] args) {
    Object testObject = createTestObject();
    Object current = testObject;
    for (int i = 0; i < PATH_LENGTH; i++) {
      current = new Object[]{current};
    }

    // should not hang or crash vm
    IdeaNativeAgentProxy.findPathsToClosestGcRoots(testObject, Integer.MAX_VALUE, Integer.MAX_VALUE, TestBase.DEFAULT_TIMEOUT, TestBase.DEFAULT_CANCELLATION_FILE);
  }
}
