//
// Source code recreated from a .class file by IntelliJ IDEA
// (powered by FernFlower decompiler)
//

package indi.wookat.quickgrab.controller;

import indi.wookat.quickgrab.entity.Buyers;
import indi.wookat.quickgrab.mapper.BuyersMapper;
import indi.wookat.quickgrab.service.StatisticsService;
import jakarta.annotation.Resource;
import java.time.LocalDateTime;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.stream.Collectors;
import org.springframework.format.annotation.DateTimeFormat;
import org.springframework.format.annotation.DateTimeFormat.ISO;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RestController;

@RestController
@RequestMapping({"/api"})
public class StatisticsController {
    @Resource
    private StatisticsService statisticsService;
    @Resource
    private BuyersMapper buyersMapper;

    @GetMapping({"/statistics"})
    public Map<String, Object> getStatistics(@RequestParam(value = "buyerId",required = false) Integer buyerId, @RequestParam(required = false) @DateTimeFormat(iso = ISO.DATE_TIME) LocalDateTime startTime, @RequestParam(required = false) @DateTimeFormat(iso = ISO.DATE_TIME) LocalDateTime endTime) {
        List<Map<String, Object>> stats = this.statisticsService.getStatistics(buyerId, startTime, endTime);
        Map<String, Object> overallStats = (Map)stats.stream().filter((stat) -> stat.get("type") == null).findFirst().orElse(new HashMap());
        overallStats.put("typeStats", stats.stream().filter((stat) -> stat.get("type") != null).collect(Collectors.toList()));
        return overallStats;
    }

    @GetMapping({"/dailyStats"})
    public List<Map<String, Object>> getDailyStats(@RequestParam(value = "buyerId",required = false) Integer buyerId, @RequestParam(value = "status",required = false) Integer status) {
        return this.statisticsService.getDailyStats(buyerId, status);
    }

    @GetMapping({"/hourlyStats"})
    public List<Map<String, Object>> getHourlyStats(@RequestParam(value = "buyerId",required = false) Integer buyerId, @RequestParam(value = "status",required = false) Integer status) {
        return this.statisticsService.getHourlyStats(buyerId, status);
    }

    @GetMapping({"/buyers"})
    public List<Buyers> getAllBuyers() {
        return this.buyersMapper.getAllBuyers();
    }
}
