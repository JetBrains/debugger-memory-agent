package execution;

import com.intellij.memory.agent.IdeaNativeAgentProxy;
import common.TestTreeNode;
import common.ExecutionTestBase;

public class RetainedSizeByClassesForObject extends ExecutionTestBase {
    @Override
    protected MemoryAgentErrorCode executeOperation(IdeaNativeAgentProxy proxy) {
        Object testObject = new Object();
        Object target = new Object[]{null, new Object[]{1, testObject}, 1, 2};
        return getErrorCode(proxy.getRetainedSizeByClasses(new Object[] {Object.class}));
    }

    public static void main(String[] args) {
        ExecutionTestBase test = new RetainedSizeByClassesForObject();
        test.doTest();
    }
}
