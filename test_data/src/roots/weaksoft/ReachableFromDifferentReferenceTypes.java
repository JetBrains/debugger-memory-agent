package roots.weaksoft;

import com.intellij.memory.agent.proxy.IdeaNativeAgentProxy;
import common.TestBase;

import java.lang.ref.PhantomReference;
import java.lang.ref.ReferenceQueue;
import java.lang.ref.SoftReference;
import java.lang.ref.WeakReference;

public class ReachableFromDifferentReferenceTypes extends TestBase {
    public static void main(String[] args) {
        WeakReference<Object> ref1 = new WeakReference<>(createTestObject());
        doPrintGcRoots(IdeaNativeAgentProxy.findPathsToClosestGcRoots(ref1.get(), 1, 1000, TestBase.DEFAULT_TIMEOUT));

        SoftReference<Object> ref2 = new SoftReference<>(createTestObject());
        doPrintGcRoots(IdeaNativeAgentProxy.findPathsToClosestGcRoots(ref2.get(), 1, 1000, TestBase.DEFAULT_TIMEOUT));

        PhantomReference<Object> ref3 = new PhantomReference<>(ref1.get(), new ReferenceQueue<>());
        doPrintGcRoots(IdeaNativeAgentProxy.findPathsToClosestGcRoots(ref1.get(), 2, 1000, TestBase.DEFAULT_TIMEOUT));
    }
}
