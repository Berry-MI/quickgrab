/*
 * Decompiled with CFR 0.152.
 * 
 * Could not load the following classes:
 *  indi.wookat.quickgrab.mapper.ResultsMapper
 *  indi.wookat.quickgrab.service.StatisticsService
 *  jakarta.annotation.Resource
 *  org.springframework.stereotype.Service
 */
package indi.wookat.quickgrab.service;

import indi.wookat.quickgrab.mapper.ResultsMapper;
import jakarta.annotation.Resource;
import java.time.LocalDateTime;
import java.util.List;
import java.util.Map;
import org.springframework.stereotype.Service;

@Service
public class StatisticsService {
    @Resource
    private ResultsMapper resultsMapper;

    public List<Map<String, Object>> getStatistics(Integer buyerId, LocalDateTime startTime, LocalDateTime endTime) {
        return this.resultsMapper.getStatistics(buyerId, startTime, endTime);
    }

    public List<Map<String, Object>> getDailyStats(Integer buyerId, Integer status) {
        return this.resultsMapper.getDailyStats(buyerId, status);
    }

    public List<Map<String, Object>> getHourlyStats(Integer buyerId, Integer status) {
        return this.resultsMapper.getHourlyStats(buyerId, status);
    }
}

