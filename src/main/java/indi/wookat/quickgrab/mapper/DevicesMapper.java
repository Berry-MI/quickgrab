/*
 * Decompiled with CFR 0.152.
 * 
 * Could not load the following classes:
 *  indi.wookat.quickgrab.entity.Devices
 *  indi.wookat.quickgrab.mapper.DevicesMapper
 */
package indi.wookat.quickgrab.mapper;

import indi.wookat.quickgrab.entity.Devices;

public interface DevicesMapper {
    public int deleteByPrimaryKey(Long var1);

    public int insert(Devices var1);

    public int insertSelective(Devices var1);

    public Devices selectByPrimaryKey(Long var1);

    public int updateByPrimaryKeySelective(Devices var1);

    public int updateByPrimaryKey(Devices var1);
}

