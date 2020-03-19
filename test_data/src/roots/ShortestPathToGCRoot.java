package roots;

import com.intellij.memory.agent.proxy.IdeaNativeAgentProxy;
import common.TestBase;
import common.TestNode;

public class ShortestPathToGCRoot extends TestBase {
    public static void main(String[] args) {
        TestNode root1 = new TestNode(10);
        TestNode root2 = new TestNode(7, root1.getLast());
        TestNode root3 = new TestNode(5, root1.getLast());
        doPrintGcRoots(IdeaNativeAgentProxy.closestGcRoot(root1.getLast()));
    }
}
