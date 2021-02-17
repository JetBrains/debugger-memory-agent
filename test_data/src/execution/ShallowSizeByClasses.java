package execution;

import com.intellij.memory.agent.IdeaNativeAgentProxy;
import common.TestTreeNode;
import common.ExecutionTestBase;

public class ShallowSizeByClasses extends ExecutionTestBase {
    @Override
    protected MemoryAgentErrorCode executeOperation(IdeaNativeAgentProxy proxy) {
        TestTreeNode root = TestTreeNode.createTreeFromString("2 1 1 0 0 0 0");
        return getErrorCode(proxy.getShallowSizeByClasses(new Object[] {TestTreeNode.Impl2.class}));
    }

    public static void main(String[] args) {
        ExecutionTestBase test = new ShallowSizeByClasses();
        test.doTest();
    }
}

