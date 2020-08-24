package size.classes.retained;

import common.TestBase;
import common.TestTreeNode;

public class ThreeClassesWithCommonSubtree2 extends TestBase {
    public static void main(String[] args) {
        /*
                          2       3
                      /      \   /
                    1          1     4
                  /   \      /   \ /
                 1 <-- 1 <- 1 <-- 1
                / \
               1   1
        */
        TestTreeNode root3 = new TestTreeNode.Impl3();
        TestTreeNode root4 = new TestTreeNode.Impl4();
        TestTreeNode root2 = TestTreeNode.createTreeFromString("2 1 1 1 0 0 1 0 0 1 0 0 1 1 0 0 1 0 0");
        root2.left.right.left = root2.left.left;
        root2.right.left.left = root2.left.right;
        root2.right.right.left = root2.right.left;
        root3.left = root2.right;
        root4.left = root2.right.right;
        printShallowAndRetainedSizeByClasses(
                TestTreeNode.Impl2.class,
                TestTreeNode.Impl3.class,
                TestTreeNode.Impl4.class
        );
    }
}
