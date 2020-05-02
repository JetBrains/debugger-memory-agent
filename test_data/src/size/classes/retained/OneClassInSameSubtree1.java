package size.classes.retained;

import common.TestBase;
import common.TestTreeNode;

public class OneClassInSameSubtree1 extends TestBase {
    public static void main(String[] args) {
        /*
                     1
                   /   \
                  2     1
                 / \
                2   1
               / \
              1   1
        */
        TestTreeNode root = TestTreeNode.createTreeFromString("1 2 2 1 0 0 1 0 0 1 0 0 1 0 0");
        printShallowAndRetainedSizeByClasses(TestTreeNode.Impl2.class);
    }
}
