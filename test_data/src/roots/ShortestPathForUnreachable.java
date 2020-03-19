package roots;

import com.intellij.memory.agent.proxy.IdeaNativeAgentProxy;
import common.TestBase;
import common.TestNode;

public class ShortestPathForUnreachable extends TestBase {
    public static void main(String[] args) {
        doPrintGcRoots(IdeaNativeAgentProxy.closestGcRoot(new TestNode(3)));
    }
}
