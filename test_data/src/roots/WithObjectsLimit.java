package roots;

import com.intellij.memory.agent.proxy.IdeaNativeAgentProxy;
import common.TestBase;
import common.TestNode;

public class WithObjectsLimit extends TestBase {
    public static void main(String[] args) {
        TestNode root1 = new TestNode(10);
        doPrintGcRoots(IdeaNativeAgentProxy.findPathsToClosestGcRoots(root1.getLast(), 5, 10, TestBase.DEFAULT_TIMEOUT));
        TestNode root2 = new TestNode(5000, root1.getLast());
        TestNode root3 = new TestNode(1000, root1.getLast());
        doPrintGcRoots(IdeaNativeAgentProxy.findPathsToClosestGcRoots(root1.getLast(), 5, 10, TestBase.DEFAULT_TIMEOUT));
        doPrintGcRoots(IdeaNativeAgentProxy.findPathsToClosestGcRoots(root1.getLast(), 5, 20, TestBase.DEFAULT_TIMEOUT));
    }
}
