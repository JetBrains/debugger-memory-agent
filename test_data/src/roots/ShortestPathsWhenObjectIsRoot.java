package roots;

import com.intellij.memory.agent.proxy.IdeaNativeAgentProxy;
import common.TestBase;
import common.TestNode;

public class ShortestPathsWhenObjectIsRoot extends TestBase {
    public static void main(String[] args) {
        TestNode root = new TestNode(1);
        doPrintGcRoots(IdeaNativeAgentProxy.findPathsToClosestGcRoots(root, 10));
    }
}
