package execution;

import com.intellij.memory.agent.IdeaNativeAgentProxy;
import common.TestTreeNode;
import common.ExecutionTestBase;

public class ShallowAndRetainedSizeByClassesForObject extends ExecutionTestBase {
    @Override
    protected MemoryAgentErrorCode executeOperation(IdeaNativeAgentProxy proxy) {
        Object testObject = new Object();
        Object target = new Object[]{null, new Object[]{1, testObject}, 1, 2};
        return getErrorCode(proxy.getShallowAndRetainedSizeByClasses(new Object[] {Object.class}));
    }

    public static void main(String[] args) {
        ExecutionTestBase test = new ShallowAndRetainedSizeByClassesForObject();
        test.doTest();
    }
}
