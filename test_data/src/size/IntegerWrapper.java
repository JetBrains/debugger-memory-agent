package size;

import common.TestBase;
import memory.agent.IdeaDebuggerNativeAgentClass;

public class IntegerWrapper extends TestBase {
  public static void main(String[] args) {
    System.out.println(IdeaDebuggerNativeAgentClass.size(1));
  }
}
