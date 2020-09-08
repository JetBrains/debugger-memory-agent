package size.classes.retained;

import common.TestBase;
import common.TestTreeNode;

public class TwoClassesWithCycle1 extends TestBase {
    public static void main(String[] args) {
        /*
              2         3
               \       /
                1 <-> 1
        */
        TestTreeNode root2 = TestTreeNode.createTreeFromString("2 0 1 0 0");
        TestTreeNode root3 = TestTreeNode.createTreeFromString("3 1 0 0 0");
        root2.right.right = root3.left;
        root3.left.left = root2.right;
        printShallowAndRetainedSizeByClasses(TestTreeNode.Impl2.class, TestTreeNode.Impl3.class);
    }
}

