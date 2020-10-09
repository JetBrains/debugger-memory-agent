package roots.weaksoft;

import com.intellij.memory.agent.proxy.IdeaNativeAgentProxy;
import common.TestBase;

import java.lang.ref.PhantomReference;
import java.lang.ref.ReferenceQueue;
import java.lang.ref.SoftReference;
import java.lang.ref.WeakReference;

public class UnreachableFromStrongReferences extends TestBase {
    public static void main(String[] args) {
        ReachableFromStrongReference.TestClass testClass = new ReachableFromStrongReference.TestClass(createTestObject());
        SoftReference<WeakReference<Object>> ref1 = new SoftReference<>(
                new WeakReference<>(testClass.object)
        );
        SoftReference<PhantomReference<Object>> ref2 = new SoftReference<>(
                new PhantomReference<>(testClass.object, new ReferenceQueue<>())
        );
        SoftReference<Object> ref3 = new SoftReference<>(testClass.object);
        SoftReference<ReachableFromStrongReference.TestClass> ref4 = new SoftReference<>(testClass);
        testClass = null;
        doPrintGcRoots(IdeaNativeAgentProxy.findPathsToClosestGcRoots(ref3.get(), 4, 1000, TestBase.DEFAULT_TIMEOUT, TestBase.DEFAULT_CANCELLATION_FILE));
    }
}
