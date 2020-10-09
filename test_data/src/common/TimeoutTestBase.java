package common;

import java.io.File;
import java.io.IOException;

public abstract class TimeoutTestBase extends TestBase {
    protected abstract TestBase.MemoryAgentErrorCode executeOperation(long timeoutInMillis, String cancellationFileName);

    public final void doTest() {
        long[] timeouts = new long[]{0, 1, DEFAULT_TIMEOUT};
        for (long t : timeouts) {
            System.out.println(executeOperation(t, TestBase.DEFAULT_CANCELLATION_FILE));
        }

        try {
            File file = File.createTempFile(TestBase.DEFAULT_CANCELLATION_FILE, "");
            System.out.println(executeOperation(TestBase.DEFAULT_TIMEOUT, file.getAbsolutePath()));
        } catch (IOException e) {
            e.printStackTrace();
        }
    }
}
