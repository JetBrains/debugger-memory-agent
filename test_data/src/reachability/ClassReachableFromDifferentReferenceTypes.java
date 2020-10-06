package reachability;

import common.TestBase;
import common.TestTreeNode;

import java.lang.ref.PhantomReference;
import java.lang.ref.ReferenceQueue;
import java.lang.ref.SoftReference;
import java.lang.ref.WeakReference;

public class ClassReachableFromDifferentReferenceTypes extends TestBase {
    private static TestTreeNode root;

    public static class Ref1<T> extends SoftReference<T> {
        public Ref1(T referent) {
            super(referent);
        }
    }

    public static class Ref2<T> extends WeakReference<T> {
        public Ref2(T referent) {
            super(referent);
        }
    }

    public static class Ref3<T> extends PhantomReference<T> {
        public Ref3(T referent, ReferenceQueue<? super T> q) {
            super(referent, q);
        }
    }

    public static void main(String[] args) {
        root = TestTreeNode.createTreeFromString("2 1 0 0 1 0 0");
        WeakReference<Object> ref1 = new WeakReference<>(root);
        doTest();


        root = TestTreeNode.createTreeFromString("2 1 0 0 1 0 0");
        SoftReference<Object> ref2 = new SoftReference<>(root);
        doTest();

        root = TestTreeNode.createTreeFromString("2 1 0 0 1 0 0");
        PhantomReference<Object> ref3 = new PhantomReference<>(root, new ReferenceQueue<>());
        doTest();

        root = TestTreeNode.createTreeFromString("2 1 0 0 1 0 0");
        Ref1<Object> ref4 = new Ref1<>(root);
        doTest();

        root = TestTreeNode.createTreeFromString("2 1 0 0 1 0 0");
        Ref2<Object> ref5 = new Ref2<>(root);
        doTest();

        root = TestTreeNode.createTreeFromString("2 1 0 0 1 0 0");
        Ref3<Object> ref6 = new Ref3<>(root, new ReferenceQueue<>());
        doTest();
    }

    public static void doTest() {
        printClassReachability(TestTreeNode.Impl1.class);
        printReachableObjects(TestTreeNode.Impl1.class);
        printReachableObjects(root, TestTreeNode.Impl1.class);
        printReachableObjects(TestTreeNode.class);
        root = null;
        printClassReachability(TestTreeNode.Impl1.class);
        printReachableObjects(TestTreeNode.Impl1.class);
        printReachableObjects(TestTreeNode.class);
    }
}
