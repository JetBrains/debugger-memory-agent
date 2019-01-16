package capabilities;

import com.intellij.memory.agent.proxy.IdeaNativeAgentProxy;
import common.TestBase;

public class FindGcRoots extends TestBase {
  public static void main(String[] args) {
    System.out.println(IdeaNativeAgentProxy.canFindGcRoots());
  }
}
