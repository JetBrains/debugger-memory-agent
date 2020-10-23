import com.intellij.memory.agent.proxy.IdeaNativeAgentProxy;
import common.TestBase;
import common.TestTreeNode;

public class ManyOperations extends TestBase {
    public static void main(String[] args) {
        TestTreeNode root = TestTreeNode.createTreeFromString("1 1 0 0 1 0 0");
        doPrintGcRoots(IdeaNativeAgentProxy.findPathsToClosestGcRoots(root, 5, 20, TestBase.DEFAULT_TIMEOUT, TestBase.DEFAULT_CANCELLATION_FILE));
        printSize(root);
        printSizeAndHeldObjects(root);
        printShallowAndRetainedSizeByClasses(TestTreeNode.Impl1.class);
        doPrintGcRoots(IdeaNativeAgentProxy.findPathsToClosestGcRoots(root, 5, 20, TestBase.DEFAULT_TIMEOUT, TestBase.DEFAULT_CANCELLATION_FILE));
        printShallowSizeByClasses(TestTreeNode.class);
        printRetainedSizeByClasses(TestTreeNode.class);
        printSize(root);
        printSizeAndHeldObjects(root);
    }
}
