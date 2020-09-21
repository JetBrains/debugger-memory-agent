package reachability;

import common.TestBase;
import common.TestTreeNode;

import java.lang.ref.PhantomReference;
import java.lang.ref.ReferenceQueue;
import java.lang.ref.SoftReference;
import java.lang.ref.WeakReference;

public class ClassReachableFromReferencesChain extends TestBase {
    public static void main(String[] args) {
        TestTreeNode root = TestTreeNode.createTreeFromString("2 1 0 0 1 0 0");
        SoftReference<WeakReference<Object>> ref1 = new SoftReference<>(new WeakReference<>(root));
        SoftReference<PhantomReference<Object>> ref2 = new SoftReference<>(new PhantomReference<>(root, new ReferenceQueue<>()));
        SoftReference<Object> ref3 = new SoftReference<>(root);
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
