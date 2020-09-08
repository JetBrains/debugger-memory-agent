package size.classes.retained;

import common.TestBase;
import common.TestTreeNode;

public class ThreeClassesWithCommonSubtree1 extends TestBase {
    public static void main(String[] args) {
        /*
              1   3   2
            /   \ | /   \
           1      1      1
                /   \
               1     1
        */
        TestTreeNode root1 = TestTreeNode.createTreeFromString("1 1 0 0 0");
        TestTreeNode root2 = TestTreeNode.createTreeFromString("2 0 1 0 0");
        TestTreeNode root3 = new TestTreeNode.Impl3();
        root1.right = TestTreeNode.createTreeFromString("1 1 0 0 1 0 0");
        root2.left = root1.right;
        root3.left = root1.right;
        printShallowAndRetainedSizeByClasses(
                TestTreeNode.Impl1.class,
                TestTreeNode.Impl2.class,
                TestTreeNode.Impl3.class
        );
    }
}
