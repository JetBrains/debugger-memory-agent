package roots;

import com.intellij.memory.agent.proxy.IdeaNativeAgentProxy;
import common.TestBase;
import common.TestNode;

public class ShortestPathsWithCycle extends TestBase {
    public static void main(String[] args) {
        TestNode root1 = new TestNode(5);
        TestNode root2 = new TestNode(3);
        TestNode last1 = root1.getLast();
        TestNode last2 = root2.getLast();
        last1.child = last2;
        last2.child = last1;
        last1 = null;
        last2 = null;

        doPrintGcRoots(IdeaNativeAgentProxy.findPathsToClosestGcRoots(root2.getChild(4), 10, 1000, TestBase.DEFAULT_TIMEOUT));
    }
}
