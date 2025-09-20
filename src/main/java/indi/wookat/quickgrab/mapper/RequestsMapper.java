/*
 * Decompiled with CFR 0.152.
 * 
 * Could not load the following classes:
 *  indi.wookat.quickgrab.entity.Requests
 *  indi.wookat.quickgrab.mapper.RequestsMapper
 *  org.apache.ibatis.annotations.Param
 */
package indi.wookat.quickgrab.mapper;

import indi.wookat.quickgrab.entity.Requests;
import java.util.List;
import org.apache.ibatis.annotations.Param;

public interface RequestsMapper {
    public int deleteByPrimaryKey(Integer var1);

    public int insert(Requests var1);

    public int insertSelective(Requests var1);

    public Requests selectByPrimaryKey(Long var1);

    public int updateByPrimaryKeySelective(Requests var1);

    public int updateByPrimaryKey(Requests var1);

    public List<Requests> selectAll();

    public List<Requests> selectByStatus(int var1);

    public int updateStatusById(@Param(value="id") Integer var1, @Param(value="status") Integer var2);

    public int updateThreadIdById(@Param(value="id") Integer var1, @Param(value="thread_id") String var2);

    public List<Requests> findRequestsByFilters(@Param(value="keyword") String var1, @Param(value="buyerId") String var2, @Param(value="type") Integer var3, @Param(value="status") Integer var4, @Param(value="orderColumn") String var5, @Param(value="orderDirection") String var6, @Param(value="offset") int var7, @Param(value="limit") int var8);
}

