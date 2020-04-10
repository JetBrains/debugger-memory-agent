package size.classes.retained;

import common.TestBase;
import common.TestTreeNode;
import size.classes.OneClass;

public class TwoClassesInDifferentSubtrees extends TestBase {
    public static void main(String[] args) {
        /*
                     1
                   /   \
                  2     3
                 / \   / \
                1   1 1   1
         */
        TestTreeNode root = TestTreeNode.createTreeFromString("1 2 1 0 0 1 0 0 3 1 0 0 1 0 0");
        printRetainedSizeByClasses(TestTreeNode.Impl2.class, TestTreeNode.Impl3.class, OneClass.class);
    }
}

