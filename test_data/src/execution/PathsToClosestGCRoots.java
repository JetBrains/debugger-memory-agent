package execution;

import com.intellij.memory.agent.IdeaNativeAgentProxy;
import common.TestNode;
import common.ExecutionTestBase;

public class PathsToClosestGCRoots extends ExecutionTestBase {
    @Override
    protected MemoryAgentErrorCode executeOperation(IdeaNativeAgentProxy proxy) {
        TestNode root = new TestNode(10);
        return getErrorCode(proxy.findPathsToClosestGcRoots(root, 10, 1000));
    }

    public static void main(String[] args) {
        ExecutionTestBase test = new PathsToClosestGCRoots();
        test.doTest();
    }
}

