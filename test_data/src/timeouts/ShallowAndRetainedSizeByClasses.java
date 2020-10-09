package timeouts;

import com.intellij.memory.agent.proxy.IdeaNativeAgentProxy;
import common.TestTreeNode;
import common.TimeoutTestBase;

public class ShallowAndRetainedSizeByClasses extends TimeoutTestBase {
    @Override
    protected MemoryAgentErrorCode executeOperation(long timeoutInMillis, String cancellationFileName) {
        TestTreeNode root = TestTreeNode.createTreeFromString("2 1 1 0 0 0 0");
        return getErrorCode(IdeaNativeAgentProxy.getShallowAndRetainedSizeByClasses(new Object[] {TestTreeNode.Impl2.class}, timeoutInMillis, cancellationFileName));
    }

    public static void main(String[] args) {
        TimeoutTestBase test = new ShallowAndRetainedSizeByClasses();
        test.doTest();
    }
}
