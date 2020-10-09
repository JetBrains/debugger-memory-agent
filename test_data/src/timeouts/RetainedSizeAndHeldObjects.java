package timeouts;

import com.intellij.memory.agent.proxy.IdeaNativeAgentProxy;
import common.TestTreeNode;
import common.TimeoutTestBase;

public class RetainedSizeAndHeldObjects extends TimeoutTestBase {
    @Override
    protected MemoryAgentErrorCode executeOperation(long timeoutInMillis, String cancellationFileName) {
        return getErrorCode(IdeaNativeAgentProxy.size(TestTreeNode.createTreeFromString("2 1 1 0 0 0 0"), timeoutInMillis, cancellationFileName));
    }

    public static void main(String[] args) {
        TimeoutTestBase test = new RetainedSizeAndHeldObjects();
        test.doTest();
    }
}
