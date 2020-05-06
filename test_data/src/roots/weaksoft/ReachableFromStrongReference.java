package roots.weaksoft;

import com.intellij.memory.agent.proxy.IdeaNativeAgentProxy;
import common.TestBase;

import java.lang.ref.PhantomReference;
import java.lang.ref.ReferenceQueue;
import java.lang.ref.SoftReference;
import java.lang.ref.WeakReference;

public class ReachableFromStrongReference extends TestBase {
    static class TestClass {
        public Object object;

        public TestClass(Object object) {
            this.object = object;
        }

        @Override
        public String toString() {
            return "test class";
        }
    }

    public static void main(String[] args) {
        TestClass testClass = new TestClass(createTestObject());
        SoftReference<WeakReference<Object>> ref1 = new SoftReference<>(
                new WeakReference<>(testClass.object)
        );
        SoftReference<PhantomReference<Object>> ref2 = new SoftReference<>(
                new PhantomReference<>(testClass.object, new ReferenceQueue<>())
        );
        SoftReference<Object> ref3 = new SoftReference<>(testClass.object);

        doPrintGcRoots(IdeaNativeAgentProxy.findPathsToClosestGcRoots(testClass.object, 4));
    }
}
