package roots.weaksoft;

import com.intellij.memory.agent.proxy.IdeaNativeAgentProxy;
import common.TestBase;
import reachability.ClassReachableFromDifferentReferenceTypes.Ref1;
import reachability.ClassReachableFromDifferentReferenceTypes.Ref2;
import reachability.ClassReachableFromDifferentReferenceTypes.Ref3;

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

        Ref1<Object> ref4 = new Ref1<>(createTestObject());
        doPrintGcRoots(IdeaNativeAgentProxy.findPathsToClosestGcRoots(ref4.get(), 2, 1000, TestBase.DEFAULT_TIMEOUT));

        Ref2<Object> ref5 = new Ref2<>(createTestObject());
        doPrintGcRoots(IdeaNativeAgentProxy.findPathsToClosestGcRoots(ref5.get(), 2, 1000, TestBase.DEFAULT_TIMEOUT));

        Ref3<Object> ref6 = new Ref3<>(ref1.get(), new ReferenceQueue<>());
        doPrintGcRoots(IdeaNativeAgentProxy.findPathsToClosestGcRoots(ref1.get(), 2, 1000, TestBase.DEFAULT_TIMEOUT));
    }
}
