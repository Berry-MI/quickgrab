/*
 * Decompiled with CFR 0.152.
 * 
 * Could not load the following classes:
 *  com.fasterxml.jackson.annotation.JsonRawValue
 *  indi.wookat.quickgrab.entity.Requests
 */
package indi.wookat.quickgrab.entity;

import com.fasterxml.jackson.annotation.JsonRawValue;
import java.io.Serializable;
import java.math.BigDecimal;
import java.time.LocalDateTime;

public class Requests
implements Serializable {
    private Integer id;
    private Integer deviceId;
    private Integer buyerId;
    private String threadId;
    private String link;
    private String cookies;
    private String orderInfo;
    @JsonRawValue
    private String userInfo;
    @JsonRawValue
    private String orderTemplate;
    private String message;
    private String idNumber;
    private String keyword;
    private LocalDateTime startTime;
    private LocalDateTime endTime;
    private Integer quantity;
    private Integer delay;
    private Integer frequency;
    private Integer type;
    private Integer status;
    @JsonRawValue
    private String orderParameters;
    private BigDecimal actualEarnings;
    private BigDecimal estimatedEarnings;
    @JsonRawValue
    private String extension;
    private static final long serialVersionUID = 1L;

    public Integer getId() {
        return this.id;
    }

    public Integer getDeviceId() {
        return this.deviceId;
    }

    public Integer getBuyerId() {
        return this.buyerId;
    }

    public String getThreadId() {
        return this.threadId;
    }

    public String getLink() {
        return this.link;
    }

    public String getCookies() {
        return this.cookies;
    }

    public String getOrderInfo() {
        return this.orderInfo;
    }

    public String getUserInfo() {
        return this.userInfo;
    }

    public String getOrderTemplate() {
        return this.orderTemplate;
    }

    public String getMessage() {
        return this.message;
    }

    public String getIdNumber() {
        return this.idNumber;
    }

    public String getKeyword() {
        return this.keyword;
    }

    public LocalDateTime getStartTime() {
        return this.startTime;
    }

    public LocalDateTime getEndTime() {
        return this.endTime;
    }

    public Integer getQuantity() {
        return this.quantity;
    }

    public Integer getDelay() {
        return this.delay;
    }

    public Integer getFrequency() {
        return this.frequency;
    }

    public Integer getType() {
        return this.type;
    }

    public Integer getStatus() {
        return this.status;
    }

    public String getOrderParameters() {
        return this.orderParameters;
    }

    public BigDecimal getActualEarnings() {
        return this.actualEarnings;
    }

    public BigDecimal getEstimatedEarnings() {
        return this.estimatedEarnings;
    }

    public String getExtension() {
        return this.extension;
    }

    public void setId(Integer id) {
        this.id = id;
    }

    public void setDeviceId(Integer deviceId) {
        this.deviceId = deviceId;
    }

    public void setBuyerId(Integer buyerId) {
        this.buyerId = buyerId;
    }

    public void setThreadId(String threadId) {
        this.threadId = threadId;
    }

    public void setLink(String link) {
        this.link = link;
    }

    public void setCookies(String cookies) {
        this.cookies = cookies;
    }

    public void setOrderInfo(String orderInfo) {
        this.orderInfo = orderInfo;
    }

    public void setUserInfo(String userInfo) {
        this.userInfo = userInfo;
    }

    public void setOrderTemplate(String orderTemplate) {
        this.orderTemplate = orderTemplate;
    }

    public void setMessage(String message) {
        this.message = message;
    }

    public void setIdNumber(String idNumber) {
        this.idNumber = idNumber;
    }

    public void setKeyword(String keyword) {
        this.keyword = keyword;
    }

    public void setStartTime(LocalDateTime startTime) {
        this.startTime = startTime;
    }

    public void setEndTime(LocalDateTime endTime) {
        this.endTime = endTime;
    }

    public void setQuantity(Integer quantity) {
        this.quantity = quantity;
    }

    public void setDelay(Integer delay) {
        this.delay = delay;
    }

    public void setFrequency(Integer frequency) {
        this.frequency = frequency;
    }

    public void setType(Integer type) {
        this.type = type;
    }

    public void setStatus(Integer status) {
        this.status = status;
    }

    public void setOrderParameters(String orderParameters) {
        this.orderParameters = orderParameters;
    }

    public void setActualEarnings(BigDecimal actualEarnings) {
        this.actualEarnings = actualEarnings;
    }

    public void setEstimatedEarnings(BigDecimal estimatedEarnings) {
        this.estimatedEarnings = estimatedEarnings;
    }

    public void setExtension(String extension) {
        this.extension = extension;
    }

    public boolean equals(Object o) {
        if (o == this) {
            return true;
        }
        if (!(o instanceof Requests)) {
            return false;
        }
        Requests other = (Requests)o;
        if (!other.canEqual((Object)this)) {
            return false;
        }
        Integer this$id = this.getId();
        Integer other$id = other.getId();
        if (this$id == null ? other$id != null : !((Object)this$id).equals(other$id)) {
            return false;
        }
        Integer this$deviceId = this.getDeviceId();
        Integer other$deviceId = other.getDeviceId();
        if (this$deviceId == null ? other$deviceId != null : !((Object)this$deviceId).equals(other$deviceId)) {
            return false;
        }
        Integer this$buyerId = this.getBuyerId();
        Integer other$buyerId = other.getBuyerId();
        if (this$buyerId == null ? other$buyerId != null : !((Object)this$buyerId).equals(other$buyerId)) {
            return false;
        }
        Integer this$quantity = this.getQuantity();
        Integer other$quantity = other.getQuantity();
        if (this$quantity == null ? other$quantity != null : !((Object)this$quantity).equals(other$quantity)) {
            return false;
        }
        Integer this$delay = this.getDelay();
        Integer other$delay = other.getDelay();
        if (this$delay == null ? other$delay != null : !((Object)this$delay).equals(other$delay)) {
            return false;
        }
        Integer this$frequency = this.getFrequency();
        Integer other$frequency = other.getFrequency();
        if (this$frequency == null ? other$frequency != null : !((Object)this$frequency).equals(other$frequency)) {
            return false;
        }
        Integer this$type = this.getType();
        Integer other$type = other.getType();
        if (this$type == null ? other$type != null : !((Object)this$type).equals(other$type)) {
            return false;
        }
        Integer this$status = this.getStatus();
        Integer other$status = other.getStatus();
        if (this$status == null ? other$status != null : !((Object)this$status).equals(other$status)) {
            return false;
        }
        String this$threadId = this.getThreadId();
        String other$threadId = other.getThreadId();
        if (this$threadId == null ? other$threadId != null : !this$threadId.equals(other$threadId)) {
            return false;
        }
        String this$link = this.getLink();
        String other$link = other.getLink();
        if (this$link == null ? other$link != null : !this$link.equals(other$link)) {
            return false;
        }
        String this$cookies = this.getCookies();
        String other$cookies = other.getCookies();
        if (this$cookies == null ? other$cookies != null : !this$cookies.equals(other$cookies)) {
            return false;
        }
        String this$orderInfo = this.getOrderInfo();
        String other$orderInfo = other.getOrderInfo();
        if (this$orderInfo == null ? other$orderInfo != null : !this$orderInfo.equals(other$orderInfo)) {
            return false;
        }
        String this$userInfo = this.getUserInfo();
        String other$userInfo = other.getUserInfo();
        if (this$userInfo == null ? other$userInfo != null : !this$userInfo.equals(other$userInfo)) {
            return false;
        }
        String this$orderTemplate = this.getOrderTemplate();
        String other$orderTemplate = other.getOrderTemplate();
        if (this$orderTemplate == null ? other$orderTemplate != null : !this$orderTemplate.equals(other$orderTemplate)) {
            return false;
        }
        String this$message = this.getMessage();
        String other$message = other.getMessage();
        if (this$message == null ? other$message != null : !this$message.equals(other$message)) {
            return false;
        }
        String this$idNumber = this.getIdNumber();
        String other$idNumber = other.getIdNumber();
        if (this$idNumber == null ? other$idNumber != null : !this$idNumber.equals(other$idNumber)) {
            return false;
        }
        String this$keyword = this.getKeyword();
        String other$keyword = other.getKeyword();
        if (this$keyword == null ? other$keyword != null : !this$keyword.equals(other$keyword)) {
            return false;
        }
        LocalDateTime this$startTime = this.getStartTime();
        LocalDateTime other$startTime = other.getStartTime();
        if (this$startTime == null ? other$startTime != null : !((Object)this$startTime).equals(other$startTime)) {
            return false;
        }
        LocalDateTime this$endTime = this.getEndTime();
        LocalDateTime other$endTime = other.getEndTime();
        if (this$endTime == null ? other$endTime != null : !((Object)this$endTime).equals(other$endTime)) {
            return false;
        }
        String this$orderParameters = this.getOrderParameters();
        String other$orderParameters = other.getOrderParameters();
        if (this$orderParameters == null ? other$orderParameters != null : !this$orderParameters.equals(other$orderParameters)) {
            return false;
        }
        BigDecimal this$actualEarnings = this.getActualEarnings();
        BigDecimal other$actualEarnings = other.getActualEarnings();
        if (this$actualEarnings == null ? other$actualEarnings != null : !((Object)this$actualEarnings).equals(other$actualEarnings)) {
            return false;
        }
        BigDecimal this$estimatedEarnings = this.getEstimatedEarnings();
        BigDecimal other$estimatedEarnings = other.getEstimatedEarnings();
        if (this$estimatedEarnings == null ? other$estimatedEarnings != null : !((Object)this$estimatedEarnings).equals(other$estimatedEarnings)) {
            return false;
        }
        String this$extension = this.getExtension();
        String other$extension = other.getExtension();
        return !(this$extension == null ? other$extension != null : !this$extension.equals(other$extension));
    }

    protected boolean canEqual(Object other) {
        return other instanceof Requests;
    }

    public int hashCode() {
        int PRIME = 59;
        int result = 1;
        Integer $id = this.getId();
        result = result * 59 + ($id == null ? 43 : ((Object)$id).hashCode());
        Integer $deviceId = this.getDeviceId();
        result = result * 59 + ($deviceId == null ? 43 : ((Object)$deviceId).hashCode());
        Integer $buyerId = this.getBuyerId();
        result = result * 59 + ($buyerId == null ? 43 : ((Object)$buyerId).hashCode());
        Integer $quantity = this.getQuantity();
        result = result * 59 + ($quantity == null ? 43 : ((Object)$quantity).hashCode());
        Integer $delay = this.getDelay();
        result = result * 59 + ($delay == null ? 43 : ((Object)$delay).hashCode());
        Integer $frequency = this.getFrequency();
        result = result * 59 + ($frequency == null ? 43 : ((Object)$frequency).hashCode());
        Integer $type = this.getType();
        result = result * 59 + ($type == null ? 43 : ((Object)$type).hashCode());
        Integer $status = this.getStatus();
        result = result * 59 + ($status == null ? 43 : ((Object)$status).hashCode());
        String $threadId = this.getThreadId();
        result = result * 59 + ($threadId == null ? 43 : $threadId.hashCode());
        String $link = this.getLink();
        result = result * 59 + ($link == null ? 43 : $link.hashCode());
        String $cookies = this.getCookies();
        result = result * 59 + ($cookies == null ? 43 : $cookies.hashCode());
        String $orderInfo = this.getOrderInfo();
        result = result * 59 + ($orderInfo == null ? 43 : $orderInfo.hashCode());
        String $userInfo = this.getUserInfo();
        result = result * 59 + ($userInfo == null ? 43 : $userInfo.hashCode());
        String $orderTemplate = this.getOrderTemplate();
        result = result * 59 + ($orderTemplate == null ? 43 : $orderTemplate.hashCode());
        String $message = this.getMessage();
        result = result * 59 + ($message == null ? 43 : $message.hashCode());
        String $idNumber = this.getIdNumber();
        result = result * 59 + ($idNumber == null ? 43 : $idNumber.hashCode());
        String $keyword = this.getKeyword();
        result = result * 59 + ($keyword == null ? 43 : $keyword.hashCode());
        LocalDateTime $startTime = this.getStartTime();
        result = result * 59 + ($startTime == null ? 43 : ((Object)$startTime).hashCode());
        LocalDateTime $endTime = this.getEndTime();
        result = result * 59 + ($endTime == null ? 43 : ((Object)$endTime).hashCode());
        String $orderParameters = this.getOrderParameters();
        result = result * 59 + ($orderParameters == null ? 43 : $orderParameters.hashCode());
        BigDecimal $actualEarnings = this.getActualEarnings();
        result = result * 59 + ($actualEarnings == null ? 43 : ((Object)$actualEarnings).hashCode());
        BigDecimal $estimatedEarnings = this.getEstimatedEarnings();
        result = result * 59 + ($estimatedEarnings == null ? 43 : ((Object)$estimatedEarnings).hashCode());
        String $extension = this.getExtension();
        result = result * 59 + ($extension == null ? 43 : $extension.hashCode());
        return result;
    }

    public String toString() {
        return "Requests(id=" + this.getId() + ", deviceId=" + this.getDeviceId() + ", buyerId=" + this.getBuyerId() + ", threadId=" + this.getThreadId() + ", link=" + this.getLink() + ", cookies=" + this.getCookies() + ", orderInfo=" + this.getOrderInfo() + ", userInfo=" + this.getUserInfo() + ", orderTemplate=" + this.getOrderTemplate() + ", message=" + this.getMessage() + ", idNumber=" + this.getIdNumber() + ", keyword=" + this.getKeyword() + ", startTime=" + this.getStartTime() + ", endTime=" + this.getEndTime() + ", quantity=" + this.getQuantity() + ", delay=" + this.getDelay() + ", frequency=" + this.getFrequency() + ", type=" + this.getType() + ", status=" + this.getStatus() + ", orderParameters=" + this.getOrderParameters() + ", actualEarnings=" + this.getActualEarnings() + ", estimatedEarnings=" + this.getEstimatedEarnings() + ", extension=" + this.getExtension() + ")";
    }
}

