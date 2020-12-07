package timeouts;

import com.intellij.memory.agent.IdeaNativeAgentProxy;
import common.TestTreeNode;
import common.TimeoutTestBase;

public class RetainedSizeAndHeldObjects extends TimeoutTestBase {
    @Override
    protected MemoryAgentErrorCode executeOperation(IdeaNativeAgentProxy proxy) {
        return getErrorCode(proxy.size(TestTreeNode.createTreeFromString("2 1 1 0 0 0 0")));
    }

    public static void main(String[] args) {
        TimeoutTestBase test = new RetainedSizeAndHeldObjects();
        test.doTest();
    }
}
