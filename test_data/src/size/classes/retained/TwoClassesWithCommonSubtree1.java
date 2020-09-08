package size.classes.retained;

import common.TestBase;
import common.TestTreeNode;

import java.util.jar.Attributes;

public class TwoClassesWithCommonSubtree1 extends TestBase {
    public static void main(String[] args) {
        /*
               3     2
             /   \ /   \
            1     1     1
                /   \
               1     1
              / \   / \
             1   1 1   1
        */
        TestTreeNode root1 = TestTreeNode.createTreeFromString("1 1 1 0 0 1 0 0 1 1 0 0 1 0 0");
        TestTreeNode root2 = TestTreeNode.createTreeFromString("2 0 1 0 0");
        TestTreeNode root3 = TestTreeNode.createTreeFromString("3 1 0 0 0");
        root3.right = root1;
        root2.left = root1;
        printShallowAndRetainedSizeByClasses(TestTreeNode.Impl2.class, TestTreeNode.Impl3.class);
        root1 = null;
        printShallowAndRetainedSizeByClasses(TestTreeNode.Impl2.class, TestTreeNode.Impl3.class);
    }
}
