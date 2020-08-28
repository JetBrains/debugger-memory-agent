package roots;

import com.intellij.memory.agent.proxy.IdeaNativeAgentProxy;
import common.TestBase;
import common.TestNode;

public class PathsToThreeRoots extends TestBase {
    public static void main(String[] args) {
        TestNode root1 = new TestNode(10);
        TestNode root2 = new TestNode(7, root1.getLast());
        TestNode root3 = new TestNode(5, root1.getLast());
        doPrintGcRoots(IdeaNativeAgentProxy.findPathsToClosestGcRoots(root1.getLast(), 1, 1000));
        doPrintGcRoots(IdeaNativeAgentProxy.findPathsToClosestGcRoots(root1.getLast(), 2, 1000));
        doPrintGcRoots(IdeaNativeAgentProxy.findPathsToClosestGcRoots(root1.getLast(), 10, 1000));
    }
}
