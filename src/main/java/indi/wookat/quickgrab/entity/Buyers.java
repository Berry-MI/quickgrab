/*
 * Decompiled with CFR 0.152.
 * 
 * Could not load the following classes:
 *  indi.wookat.quickgrab.entity.Buyers
 */
package indi.wookat.quickgrab.entity;

import java.io.Serializable;
import java.util.Date;

public class Buyers
implements Serializable {
    private Integer id;
    private String username;
    private String password;
    private String email;
    private Integer accessLevel;
    private Integer dailyMaxSubmissions;
    private Integer dailySubmissionCount;
    private Date validityPeriod;
    private static final long serialVersionUID = 1L;

    public Integer getId() {
        return this.id;
    }

    public String getUsername() {
        return this.username;
    }

    public String getPassword() {
        return this.password;
    }

    public String getEmail() {
        return this.email;
    }

    public Integer getAccessLevel() {
        return this.accessLevel;
    }

    public Integer getDailyMaxSubmissions() {
        return this.dailyMaxSubmissions;
    }

    public Integer getDailySubmissionCount() {
        return this.dailySubmissionCount;
    }

    public Date getValidityPeriod() {
        return this.validityPeriod;
    }

    public void setId(Integer id) {
        this.id = id;
    }

    public void setUsername(String username) {
        this.username = username;
    }

    public void setPassword(String password) {
        this.password = password;
    }

    public void setEmail(String email) {
        this.email = email;
    }

    public void setAccessLevel(Integer accessLevel) {
        this.accessLevel = accessLevel;
    }

    public void setDailyMaxSubmissions(Integer dailyMaxSubmissions) {
        this.dailyMaxSubmissions = dailyMaxSubmissions;
    }

    public void setDailySubmissionCount(Integer dailySubmissionCount) {
        this.dailySubmissionCount = dailySubmissionCount;
    }

    public void setValidityPeriod(Date validityPeriod) {
        this.validityPeriod = validityPeriod;
    }

    public boolean equals(Object o) {
        if (o == this) {
            return true;
        }
        if (!(o instanceof Buyers)) {
            return false;
        }
        Buyers other = (Buyers)o;
        if (!other.canEqual((Object)this)) {
            return false;
        }
        Integer this$id = this.getId();
        Integer other$id = other.getId();
        if (this$id == null ? other$id != null : !((Object)this$id).equals(other$id)) {
            return false;
        }
        Integer this$accessLevel = this.getAccessLevel();
        Integer other$accessLevel = other.getAccessLevel();
        if (this$accessLevel == null ? other$accessLevel != null : !((Object)this$accessLevel).equals(other$accessLevel)) {
            return false;
        }
        Integer this$dailyMaxSubmissions = this.getDailyMaxSubmissions();
        Integer other$dailyMaxSubmissions = other.getDailyMaxSubmissions();
        if (this$dailyMaxSubmissions == null ? other$dailyMaxSubmissions != null : !((Object)this$dailyMaxSubmissions).equals(other$dailyMaxSubmissions)) {
            return false;
        }
        Integer this$dailySubmissionCount = this.getDailySubmissionCount();
        Integer other$dailySubmissionCount = other.getDailySubmissionCount();
        if (this$dailySubmissionCount == null ? other$dailySubmissionCount != null : !((Object)this$dailySubmissionCount).equals(other$dailySubmissionCount)) {
            return false;
        }
        String this$username = this.getUsername();
        String other$username = other.getUsername();
        if (this$username == null ? other$username != null : !this$username.equals(other$username)) {
            return false;
        }
        String this$password = this.getPassword();
        String other$password = other.getPassword();
        if (this$password == null ? other$password != null : !this$password.equals(other$password)) {
            return false;
        }
        String this$email = this.getEmail();
        String other$email = other.getEmail();
        if (this$email == null ? other$email != null : !this$email.equals(other$email)) {
            return false;
        }
        Date this$validityPeriod = this.getValidityPeriod();
        Date other$validityPeriod = other.getValidityPeriod();
        return !(this$validityPeriod == null ? other$validityPeriod != null : !((Object)this$validityPeriod).equals(other$validityPeriod));
    }

    protected boolean canEqual(Object other) {
        return other instanceof Buyers;
    }

    public int hashCode() {
        int PRIME = 59;
        int result = 1;
        Integer $id = this.getId();
        result = result * 59 + ($id == null ? 43 : ((Object)$id).hashCode());
        Integer $accessLevel = this.getAccessLevel();
        result = result * 59 + ($accessLevel == null ? 43 : ((Object)$accessLevel).hashCode());
        Integer $dailyMaxSubmissions = this.getDailyMaxSubmissions();
        result = result * 59 + ($dailyMaxSubmissions == null ? 43 : ((Object)$dailyMaxSubmissions).hashCode());
        Integer $dailySubmissionCount = this.getDailySubmissionCount();
        result = result * 59 + ($dailySubmissionCount == null ? 43 : ((Object)$dailySubmissionCount).hashCode());
        String $username = this.getUsername();
        result = result * 59 + ($username == null ? 43 : $username.hashCode());
        String $password = this.getPassword();
        result = result * 59 + ($password == null ? 43 : $password.hashCode());
        String $email = this.getEmail();
        result = result * 59 + ($email == null ? 43 : $email.hashCode());
        Date $validityPeriod = this.getValidityPeriod();
        result = result * 59 + ($validityPeriod == null ? 43 : ((Object)$validityPeriod).hashCode());
        return result;
    }

    public String toString() {
        return "Buyers(id=" + this.getId() + ", username=" + this.getUsername() + ", password=" + this.getPassword() + ", email=" + this.getEmail() + ", accessLevel=" + this.getAccessLevel() + ", dailyMaxSubmissions=" + this.getDailyMaxSubmissions() + ", dailySubmissionCount=" + this.getDailySubmissionCount() + ", validityPeriod=" + this.getValidityPeriod() + ")";
    }
}

