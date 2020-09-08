package size.classes.retained;

import common.TestBase;
import common.TestTreeNode;

public class TwoClassesWithCycle2 extends TestBase {
    public static void main(String[] args) {
        /*
                  2
                /   \
               1 <-- 1
             /   \
            1 --> 1 <-- 3
        */
        TestTreeNode root2 = TestTreeNode.createTreeFromString("2 1 1 0 0 1 0 0 1 0 0");
        TestTreeNode root3 = new TestTreeNode.Impl3();
        root3.left = root2.left.right;
        root2.right.left = root2.left;
        root2.left.left.right = root2.left.right;
        root2.left.right.left = root2.left;
        printShallowAndRetainedSizeByClasses(TestTreeNode.Impl2.class, TestTreeNode.Impl3.class);
    }
}
