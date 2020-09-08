package size.classes.retained;

import common.TestBase;
import common.TestTreeNode;

public class TwoClassesWithCommonSubtree2 extends TestBase {
    public static void main(String[] args) {
        /*
                  2
                /   \
               1     1   3
              / \   / \ / \
             1   1 1   1   1
                      / \
                     1   1
        */
        TestTreeNode root2 = TestTreeNode.createTreeFromString("2 1 1 0 0 1 0 0 1 1 0 0 1 1 0 0 1 0 0");
        TestTreeNode root3 = TestTreeNode.createTreeFromString("3 0 1 0 0");
        root3.left = root2.right.right;
        printShallowAndRetainedSizeByClasses(TestTreeNode.Impl2.class, TestTreeNode.Impl3.class);
    }
}

