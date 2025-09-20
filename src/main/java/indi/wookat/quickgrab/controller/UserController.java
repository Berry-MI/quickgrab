/*
 * Decompiled with CFR 0.152.
 * 
 * Could not load the following classes:
 *  indi.wookat.quickgrab.controller.UserController
 *  indi.wookat.quickgrab.entity.Buyers
 *  indi.wookat.quickgrab.mapper.BuyersMapper
 *  jakarta.annotation.Resource
 *  org.springframework.security.core.Authentication
 *  org.springframework.web.bind.annotation.GetMapping
 *  org.springframework.web.bind.annotation.RequestMapping
 *  org.springframework.web.bind.annotation.RestController
 */
package indi.wookat.quickgrab.controller;

import indi.wookat.quickgrab.entity.Buyers;
import indi.wookat.quickgrab.mapper.BuyersMapper;
import jakarta.annotation.Resource;
import java.util.HashMap;
import java.util.Map;
import org.springframework.security.core.Authentication;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

@RestController
@RequestMapping(value={"/api"})
public class UserController {
    @Resource
    private BuyersMapper buyersMapper;

    @GetMapping(value={"/user"})
    public Map<String, Object> getUserInfo(Authentication authentication) {
        String username = authentication.getName();
        Buyers buyer = this.buyersMapper.selectByUsername(username);
        HashMap<String, Object> userInfo = new HashMap<String, Object>();
        userInfo.put("username", buyer.getUsername());
        userInfo.put("accessLevel", buyer.getAccessLevel());
        userInfo.put("email", buyer.getEmail());
        return userInfo;
    }
}

