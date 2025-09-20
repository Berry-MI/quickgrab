/*
 * Decompiled with CFR 0.152.
 * 
 * Could not load the following classes:
 *  indi.wookat.quickgrab.entity.Devices
 */
package indi.wookat.quickgrab.entity;

import java.io.Serializable;

public class Devices
implements Serializable {
    private Integer id;
    private String ipAddress;
    private Integer maxConcurrent;
    private Integer priority;
    private String info;
    private static final long serialVersionUID = 1L;

    public Integer getId() {
        return this.id;
    }

    public String getIpAddress() {
        return this.ipAddress;
    }

    public Integer getMaxConcurrent() {
        return this.maxConcurrent;
    }

    public Integer getPriority() {
        return this.priority;
    }

    public String getInfo() {
        return this.info;
    }

    public void setId(Integer id) {
        this.id = id;
    }

    public void setIpAddress(String ipAddress) {
        this.ipAddress = ipAddress;
    }

    public void setMaxConcurrent(Integer maxConcurrent) {
        this.maxConcurrent = maxConcurrent;
    }

    public void setPriority(Integer priority) {
        this.priority = priority;
    }

    public void setInfo(String info) {
        this.info = info;
    }

    public boolean equals(Object o) {
        if (o == this) {
            return true;
        }
        if (!(o instanceof Devices)) {
            return false;
        }
        Devices other = (Devices)o;
        if (!other.canEqual((Object)this)) {
            return false;
        }
        Integer this$id = this.getId();
        Integer other$id = other.getId();
        if (this$id == null ? other$id != null : !((Object)this$id).equals(other$id)) {
            return false;
        }
        Integer this$maxConcurrent = this.getMaxConcurrent();
        Integer other$maxConcurrent = other.getMaxConcurrent();
        if (this$maxConcurrent == null ? other$maxConcurrent != null : !((Object)this$maxConcurrent).equals(other$maxConcurrent)) {
            return false;
        }
        Integer this$priority = this.getPriority();
        Integer other$priority = other.getPriority();
        if (this$priority == null ? other$priority != null : !((Object)this$priority).equals(other$priority)) {
            return false;
        }
        String this$ipAddress = this.getIpAddress();
        String other$ipAddress = other.getIpAddress();
        if (this$ipAddress == null ? other$ipAddress != null : !this$ipAddress.equals(other$ipAddress)) {
            return false;
        }
        String this$info = this.getInfo();
        String other$info = other.getInfo();
        return !(this$info == null ? other$info != null : !this$info.equals(other$info));
    }

    protected boolean canEqual(Object other) {
        return other instanceof Devices;
    }

    public int hashCode() {
        int PRIME = 59;
        int result = 1;
        Integer $id = this.getId();
        result = result * 59 + ($id == null ? 43 : ((Object)$id).hashCode());
        Integer $maxConcurrent = this.getMaxConcurrent();
        result = result * 59 + ($maxConcurrent == null ? 43 : ((Object)$maxConcurrent).hashCode());
        Integer $priority = this.getPriority();
        result = result * 59 + ($priority == null ? 43 : ((Object)$priority).hashCode());
        String $ipAddress = this.getIpAddress();
        result = result * 59 + ($ipAddress == null ? 43 : $ipAddress.hashCode());
        String $info = this.getInfo();
        result = result * 59 + ($info == null ? 43 : $info.hashCode());
        return result;
    }

    public String toString() {
        return "Devices(id=" + this.getId() + ", ipAddress=" + this.getIpAddress() + ", maxConcurrent=" + this.getMaxConcurrent() + ", priority=" + this.getPriority() + ", info=" + this.getInfo() + ")";
    }
}

