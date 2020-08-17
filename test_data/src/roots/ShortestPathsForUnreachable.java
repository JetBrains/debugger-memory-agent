package roots;

import com.intellij.memory.agent.proxy.IdeaNativeAgentProxy;
import common.TestBase;
import common.TestNode;

public class ShortestPathsForUnreachable extends TestBase {
    public static void main(String[] args) {
        doPrintGcRoots(IdeaNativeAgentProxy.findPathsToClosestGcRoots(new TestNode(3), 10));
    }
}
