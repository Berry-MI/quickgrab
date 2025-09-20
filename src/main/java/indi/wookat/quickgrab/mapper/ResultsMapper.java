/*
 * Decompiled with CFR 0.152.
 * 
 * Could not load the following classes:
 *  indi.wookat.quickgrab.entity.Results
 *  indi.wookat.quickgrab.mapper.ResultsMapper
 *  org.apache.ibatis.annotations.Param
 */
package indi.wookat.quickgrab.mapper;

import indi.wookat.quickgrab.entity.Results;
import java.time.LocalDateTime;
import java.util.List;
import java.util.Map;
import org.apache.ibatis.annotations.Param;

public interface ResultsMapper {
    public int deleteByPrimaryKey(Long var1);

    public int insert(Results var1);

    public int insertSelective(Results var1);

    public Results selectByPrimaryKey(Long var1);

    public int updateByPrimaryKeySelective(Results var1);

    public int updateByPrimaryKey(Results var1);

    public List<Results> selectAll();

    public List<Results> selectByKeyword(String var1);

    public List<Results> findResultsByFilters(@Param(value="keyword") String var1, @Param(value="buyerId") String var2, @Param(value="type") Integer var3, @Param(value="status") Integer var4, @Param(value="orderColumn") String var5, @Param(value="orderDirection") String var6, @Param(value="offset") int var7, @Param(value="limit") int var8);

    public List<Map<String, Object>> getStatistics(@Param(value="buyerId") Integer var1, @Param(value="startTime") LocalDateTime var2, @Param(value="endTime") LocalDateTime var3);

    public List<Map<String, Object>> getDailyStats(@Param(value="buyerId") Integer var1, @Param(value="status") Integer var2);

    public List<Map<String, Object>> getHourlyStats(@Param(value="buyerId") Integer var1, @Param(value="status") Integer var2);
}

