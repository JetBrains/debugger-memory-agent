package size.classes.retained;

import common.TestBase;
import common.TestTreeNode;

public class TwoClassesWithCommonSubtree3 extends TestBase {
    public static void main(String[] args) {
        /*
                  2    3
                /   \ /
               1 <-- 1
              / \
             1   1
        */
        TestTreeNode root2 = TestTreeNode.createTreeFromString("2 1 1 0 0 1 0 0 1 0 0");
        TestTreeNode root3 = new TestTreeNode.Impl3();
        root3.left = root2.right;
        root2.right.left = root2.left;
        printShallowAndRetainedSizeByClasses(TestTreeNode.Impl2.class, TestTreeNode.Impl3.class);
    }
}

