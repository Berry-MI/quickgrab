/*
 * Decompiled with CFR 0.152.
 * 
 * Could not load the following classes:
 *  indi.wookat.quickgrab.util.RetryUtil
 *  org.slf4j.Logger
 *  org.slf4j.LoggerFactory
 */
package indi.wookat.quickgrab.util;

import java.util.Random;
import java.util.concurrent.Callable;
import java.util.function.Predicate;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/*
 * Exception performing whole class analysis ignored.
 */
public class RetryUtil {
    private static final Logger logger = LoggerFactory.getLogger(RetryUtil.class);
    private static final Random random = new Random();

    public static <T> T retryWithExponentialBackoff(Callable<T> operation, Predicate<T> predicate, int maxRetries, long baseWaitTimeMs, int jitterPercent, String taskName) {
        int retryCount = 0;
        T result = null;
        Exception lastException = null;
        boolean success = false;
        do {
            try {
                result = operation.call();
                if (predicate.test(result)) {
                    success = true;
                    break;
                }
                logger.warn("{} \u6267\u884c\u7ed3\u679c\u4e0d\u6ee1\u8db3\u6761\u4ef6\uff0c\u51c6\u5907\u7b2c {}/{} \u6b21\u91cd\u8bd5", new Object[]{taskName, ++retryCount, maxRetries});
                if (retryCount > maxRetries) continue;
                long waitTime = RetryUtil.calculateWaitTime((int)retryCount, (long)baseWaitTimeMs, (int)jitterPercent);
                logger.info("{} \u7b49\u5f85 {}ms \u540e\u91cd\u8bd5", (Object)taskName, (Object)waitTime);
                Thread.sleep(waitTime);
            }
            catch (Exception e) {
                lastException = e;
                logger.error("{} \u6267\u884c\u5f02\u5e38\uff0c\u51c6\u5907\u7b2c {}/{} \u6b21\u91cd\u8bd5: {}", new Object[]{taskName, ++retryCount, maxRetries, e.getMessage()});
                if (retryCount > maxRetries) continue;
                try {
                    long waitTime = RetryUtil.calculateWaitTime((int)retryCount, (long)baseWaitTimeMs, (int)jitterPercent);
                    logger.info("{} \u7b49\u5f85 {}ms \u540e\u91cd\u8bd5", (Object)taskName, (Object)waitTime);
                    Thread.sleep(waitTime);
                }
                catch (InterruptedException ie) {
                    Thread.currentThread().interrupt();
                    logger.error("{} \u91cd\u8bd5\u7b49\u5f85\u88ab\u4e2d\u65ad: {}", (Object)taskName, (Object)ie.getMessage());
                    break;
                }
            }
        } while (retryCount <= maxRetries);
        if (!success && retryCount > maxRetries) {
            if (lastException != null) {
                logger.error("{} \u8fbe\u5230\u6700\u5927\u91cd\u8bd5\u6b21\u6570 {} \u540e\u4ecd\u5931\u8d25\uff0c\u6700\u540e\u5f02\u5e38: {}", new Object[]{taskName, maxRetries, lastException.getMessage()});
            } else {
                logger.error("{} \u8fbe\u5230\u6700\u5927\u91cd\u8bd5\u6b21\u6570 {} \u540e\u4ecd\u4e0d\u6ee1\u8db3\u6761\u4ef6", (Object)taskName, (Object)maxRetries);
            }
        }
        return result;
    }

    private static long calculateWaitTime(int retryCount, long baseWaitTimeMs, int jitterPercent) {
        long exponentialWait = baseWaitTimeMs * (long)Math.pow(2.0, retryCount - 1);
        double jitterMultiplier = 1.0 + (random.nextDouble() * (double)jitterPercent / 100.0 * 2.0 - (double)jitterPercent / 100.0);
        long waitTime = Math.max(baseWaitTimeMs, (long)((double)exponentialWait * jitterMultiplier));
        return waitTime;
    }
}

