package timeouts;

import com.intellij.memory.agent.proxy.IdeaNativeAgentProxy;
import common.TestTreeNode;
import common.TimeoutTestBase;

public class RetainedSizeByObjects extends TimeoutTestBase {
    @Override
    protected MemoryAgentErrorCode executeOperation(IdeaNativeAgentProxy proxy) {
        TestTreeNode root = TestTreeNode.createTreeFromString("2 1 1 0 0 0 0");
        return getErrorCode(proxy.estimateRetainedSize(new Object[]{root}));
    }

    public static void main(String[] args) {
        TimeoutTestBase test = new RetainedSizeByObjects();
        test.doTest();
    }
}
