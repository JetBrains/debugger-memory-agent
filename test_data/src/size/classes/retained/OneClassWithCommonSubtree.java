package size.classes.retained;

import common.TestBase;
import common.TestTreeNode;

public class OneClassWithCommonSubtree extends TestBase {
    public static void main(String[] args) {
        /*
               2     2
             /   \ /   \
            1     1     1
                /   \
               1     1
        */
        TestTreeNode root1 = TestTreeNode.createTreeFromString("2 1 0 0 0");
        TestTreeNode root2 = TestTreeNode.createTreeFromString("2 0 1 0 0");
        root1.right = TestTreeNode.createTreeFromString("1 1 0 0 1 0 0");;
        root2.left = root1.right;
        printShallowAndRetainedSizeByClasses(TestTreeNode.Impl2.class);
    }
}
