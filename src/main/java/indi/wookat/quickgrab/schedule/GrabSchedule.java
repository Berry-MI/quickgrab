//
// Source code recreated from a .class file by IntelliJ IDEA
// (powered by FernFlower decompiler)
//

package indi.wookat.quickgrab.schedule;

import com.fasterxml.jackson.core.JsonProcessingException;
import indi.wookat.quickgrab.entity.Requests;
import indi.wookat.quickgrab.mapper.RequestsMapper;
import indi.wookat.quickgrab.service.GrabService;
import jakarta.annotation.Resource;
import java.time.LocalDateTime;
import java.time.temporal.ChronoUnit;
import java.util.Random;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.scheduling.annotation.Async;
import org.springframework.scheduling.annotation.Scheduled;
import org.springframework.stereotype.Component;

@Component
public class GrabSchedule {
    private static final Logger logger = LoggerFactory.getLogger(GrabSchedule.class);
    private final ScheduledExecutorService delayedTaskExecutor = Executors.newScheduledThreadPool(20);
    @Resource
    private RequestsMapper requestsMapper;
    @Resource
    private GrabService grabService;

    @Scheduled(
            fixedRate = 5000L
    )
    @Async
    public void checkGrabRequest() {
        for(Requests request : this.requestsMapper.selectByStatus(1)) {
            long timeDiff = ChronoUnit.MILLIS.between(LocalDateTime.now(), request.getStartTime());
            if (timeDiff <= 20000L) {
                this.requestsMapper.updateStatusById(request.getId(), 2);
                if (request.getType() == 1) {
                    int randomDelay = (new Random()).nextInt(2000);
                    logger.info("ID: {} 定时开售，将在{}毫秒后执行抢购...", request.getId(), randomDelay);
                    this.delayedTaskExecutor.schedule(() -> {
                        try {
                            logger.info("ID: {} 延迟{}毫秒后开始执行抢购...", request.getId(), randomDelay);
                            this.grabService.executeGrab(request);
                        } catch (Exception e) {
                            logger.error("ID: {} 执行抢购任务出错: {}", request.getId(), e.getMessage());
                        }

                    }, (long)randomDelay, TimeUnit.MILLISECONDS);
                } else if (request.getType() == 3) {
                    logger.info("ID: {} 捡漏模式，执行抢购...", request.getId());
                    this.grabService.executePick(request);
                } else if (request.getType() == 2) {
                    logger.info("ID: {} 手动上架，执行抢购...", request.getId());
                    this.grabService.checkItems(request);
                }
            }
        }

    }

    @Scheduled(
            fixedRate = 1800000L
    )
    @Async
    public void checkRequestValid() throws JsonProcessingException {
        for(Requests request : this.requestsMapper.selectByStatus(1)) {
            long timeDiff = ChronoUnit.MILLIS.between(LocalDateTime.now(), request.getStartTime());
            long timeDiffMinutes = TimeUnit.MILLISECONDS.toMinutes(timeDiff);
            if (timeDiffMinutes >= 25L && timeDiffMinutes <= 35L && request.getType() == 1) {
                logger.info("执行订单检查");
                this.grabService.checkLinkValid(request);
            }
        }

    }

    @Scheduled(
            cron = "0 0 8 * * ?"
    )
    @Async
    public void dailyCheck() {
        try {
            logger.info("日常检查...");
        } catch (Exception e) {
            logger.error(e.toString());
        }

    }
}
