package capabilities;

import com.intellij.memory.agent.proxy.IdeaNativeAgentProxy;
import common.TestBase;

public class EstimateSize extends TestBase {
  public static void main(String[] args) {
    System.out.println(proxy.canEstimateObjectSize());
  }
}
