package common;

public class TestNode {
    public TestNode child;
    private static int num = 0;
    private final int number = ++num;

    public TestNode(int depth) {
        child = constructList(depth - 1, null);
    }

    public TestNode(int depth, TestNode leaf) {
        child = constructList(depth - 1, leaf);
    }

    private TestNode(TestNode node) {
        child = node;
    }

    private TestNode constructList(int depth, TestNode leaf) {
        if (leaf != null) {
            --depth;
        }

        TestNode node = leaf;
        while (depth-- > 0) {
            node = new TestNode(node);
        }

        return node;
    }

    public TestNode getChild(int depth) {
        TestNode node = this;
        while (--depth > 0 && node.child != null) {
            node = node.child;
        }

        return node;
    }

    public TestNode getLast() {
        TestNode node = this;
        while (node.child != null) {
            node = node.child;
        }

        return node;
    }

    @Override
    public String toString() {
        return "[node " + number + "]";
    }
}
