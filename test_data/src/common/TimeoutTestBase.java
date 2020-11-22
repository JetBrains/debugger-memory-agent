package common;

import com.intellij.memory.agent.proxy.IdeaNativeAgentProxy;

import java.io.File;
import java.io.IOException;

public abstract class TimeoutTestBase extends TestBase {
    protected abstract TestBase.MemoryAgentErrorCode executeOperation(IdeaNativeAgentProxy proxy);

    public final void doTest() {
        long[] timeouts = new long[]{0, 1, DEFAULT_TIMEOUT};
        IdeaNativeAgentProxy timeoutProxy = new IdeaNativeAgentProxy();
        for (long timeout : timeouts) {
            timeoutProxy.timeoutInMillis = timeout;
            System.out.println(executeOperation(timeoutProxy));
        }

        try {
            File file = File.createTempFile(TestBase.DEFAULT_CANCELLATION_FILE, "");
            timeoutProxy.cancellationFileName = file.getPath();
            System.out.println(executeOperation(timeoutProxy));
        } catch (IOException e) {
            e.printStackTrace();
        }
    }
}
