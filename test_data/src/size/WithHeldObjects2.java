package size;

import common.TestBase;
import common.TestTreeNode;

public class WithHeldObjects2 extends TestBase {
    public static void main(String[] args) {
        /*
                  2
                /   \
               1 <-- 1
             /  \\
            1 --> 1 <-- 3
        */
        TestTreeNode root2 = TestTreeNode.createTreeFromString("2 1 1 0 0 1 0 0 1 0 0");
        TestTreeNode root3 = new TestTreeNode.Impl3();
        root2.right.left = root2.left;
        root2.left.left.right = root2.left.right;
        root2.left.right.left = root2.left;
        printSizeAndHeldObjects(root2);
        printSizes(root2, root3, root2.right);
        root3.left = root2.left.right;
        printSizeAndHeldObjects(root2);
        printSizes(root2, root3, root2.right);
    }
}
