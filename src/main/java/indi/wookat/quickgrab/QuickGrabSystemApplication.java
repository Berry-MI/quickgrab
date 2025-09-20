/*
 * Decompiled with CFR 0.152.
 * 
 * Could not load the following classes:
 *  indi.wookat.quickgrab.QuickGrabSystemApplication
 *  org.mybatis.spring.annotation.MapperScan
 *  org.springframework.boot.SpringApplication
 *  org.springframework.boot.autoconfigure.SpringBootApplication
 *  org.springframework.scheduling.annotation.EnableAsync
 *  org.springframework.scheduling.annotation.EnableScheduling
 */
package indi.wookat.quickgrab;

import org.mybatis.spring.annotation.MapperScan;
import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;
import org.springframework.scheduling.annotation.EnableAsync;
import org.springframework.scheduling.annotation.EnableScheduling;

@SpringBootApplication
@EnableScheduling
@EnableAsync
@MapperScan(value={"indi.wookat.quickgrab.mapper"})
public class QuickGrabSystemApplication {
    public static void main(String[] args) {
        SpringApplication.run(QuickGrabSystemApplication.class, (String[])args);
    }
}

