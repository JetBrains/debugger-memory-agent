package timeouts;

import com.intellij.memory.agent.proxy.IdeaNativeAgentProxy;
import common.TestNode;
import common.TimeoutTestBase;

public class PathsToClosestGCRoots extends TimeoutTestBase {
    @Override
    protected MemoryAgentErrorCode executeOperation(IdeaNativeAgentProxy proxy) {
        TestNode root = new TestNode(10);
        return getErrorCode(proxy.findPathsToClosestGcRoots(root, 10, 1000));
    }

    public static void main(String[] args) {
        TimeoutTestBase test = new PathsToClosestGCRoots();
        test.doTest();
    }
}

