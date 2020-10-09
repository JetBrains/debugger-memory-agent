package timeouts;

import com.intellij.memory.agent.proxy.IdeaNativeAgentProxy;
import common.TestNode;
import common.TimeoutTestBase;

public class PathsToClosestGCRoots extends TimeoutTestBase {
    @Override
    protected MemoryAgentErrorCode executeOperation(long timeoutInMillis, String cancellationFileName) {
        TestNode root = new TestNode(10);
        return getErrorCode(IdeaNativeAgentProxy.findPathsToClosestGcRoots(root, 10, 1000, timeoutInMillis, cancellationFileName));
    }

    public static void main(String[] args) {
        TimeoutTestBase test = new PathsToClosestGCRoots();
        test.doTest();
    }
}

