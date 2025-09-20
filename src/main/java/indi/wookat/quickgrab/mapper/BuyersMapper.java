/*
 * Decompiled with CFR 0.152.
 * 
 * Could not load the following classes:
 *  indi.wookat.quickgrab.entity.Buyers
 *  indi.wookat.quickgrab.mapper.BuyersMapper
 */
package indi.wookat.quickgrab.mapper;

import indi.wookat.quickgrab.entity.Buyers;
import java.util.List;

public interface BuyersMapper {
    public int deleteByPrimaryKey(Long var1);

    public int insert(Buyers var1);

    public int insertSelective(Buyers var1);

    public Buyers selectByPrimaryKey(Long var1);

    public int updateByPrimaryKeySelective(Buyers var1);

    public int updateByPrimaryKey(Buyers var1);

    public List<Buyers> selectAll();

    public Buyers selectByUsername(String var1);

    public List<Buyers> getAllBuyers();
}

