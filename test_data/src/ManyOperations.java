import com.intellij.memory.agent.proxy.IdeaNativeAgentProxy;
import common.TestBase;
import common.TestTreeNode;

public class ManyOperations extends TestBase {
    public static void main(String[] args) {
        TestTreeNode root = TestTreeNode.createTreeFromString("1 1 0 0 1 0 0");
        doPrintGcRoots(proxy.findPathsToClosestGcRoots(root, 5, 20));
        printSize(root);
        printSizeAndHeldObjects(root);
        printShallowAndRetainedSizeByClasses(TestTreeNode.Impl1.class);
        doPrintGcRoots(proxy.findPathsToClosestGcRoots(root, 5, 20));
        printShallowSizeByClasses(TestTreeNode.class);
        printRetainedSizeByClasses(TestTreeNode.class);
        printSize(root);
        printSizeAndHeldObjects(root);
    }
}
