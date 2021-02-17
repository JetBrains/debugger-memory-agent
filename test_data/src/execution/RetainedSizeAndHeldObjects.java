package execution;

import com.intellij.memory.agent.IdeaNativeAgentProxy;
import common.TestTreeNode;
import common.ExecutionTestBase;

public class RetainedSizeAndHeldObjects extends ExecutionTestBase {
    @Override
    protected MemoryAgentErrorCode executeOperation(IdeaNativeAgentProxy proxy) {
        return getErrorCode(proxy.size(TestTreeNode.createTreeFromString("2 1 1 0 0 0 0")));
    }

    public static void main(String[] args) {
        ExecutionTestBase test = new RetainedSizeAndHeldObjects();
        test.doTest();
    }
}
