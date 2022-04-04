package size;

import common.TestBase;
import common.TestTreeNode;

public class WithHeldObjects1 extends TestBase {
    public static void main(String[] args) {
        /*
               2     3
             /   \ /   \
            1     1     1
                /   \
               1     1
        */
        TestTreeNode root2 = TestTreeNode.createTreeFromString("2 1 0 0 0");
        TestTreeNode root3 = TestTreeNode.createTreeFromString("3 0 1 0 0");
        root2.right = TestTreeNode.createTreeFromString("1 1 0 0 1 0 0");;
        printSizeAndHeldObjects(root2);
        root3.left = root2.right;
        printSizeAndHeldObjects(root2);
        printSizes(root2, root3, root2.right);
    }
}
