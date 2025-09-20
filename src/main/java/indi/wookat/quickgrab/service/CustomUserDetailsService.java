//
// Source code recreated from a .class file by IntelliJ IDEA
// (powered by FernFlower decompiler)
//

package indi.wookat.quickgrab.service;

import indi.wookat.quickgrab.entity.Buyers;
import indi.wookat.quickgrab.mapper.BuyersMapper;
import jakarta.annotation.Resource;
import java.util.Collections;
import java.util.Date;
import org.springframework.security.core.authority.SimpleGrantedAuthority;
import org.springframework.security.core.userdetails.User;
import org.springframework.security.core.userdetails.UserDetails;
import org.springframework.security.core.userdetails.UserDetailsService;
import org.springframework.security.core.userdetails.UsernameNotFoundException;
import org.springframework.stereotype.Service;

@Service
public class CustomUserDetailsService implements UserDetailsService {
    @Resource
    private BuyersMapper buyersMapper;
    private static final ThreadLocal<String> failureReason = new ThreadLocal();

    public UserDetails loadUserByUsername(String username) throws UsernameNotFoundException {
        System.out.println("loadUserByUsername: " + username);
        Buyers buyer = this.buyersMapper.selectByUsername(username);
        if (buyer == null) {
            failureReason.set("账号或密码错误");
            throw new UsernameNotFoundException("User not found");
        } else if (buyer.getAccessLevel() == 0) {
            failureReason.set("账号或密码错误");
            throw new UsernameNotFoundException("Account disabled");
        } else {
            Date currentDate = new Date();
            if (buyer.getValidityPeriod() != null && currentDate.after(buyer.getValidityPeriod())) {
                failureReason.set("账号已过期");
                throw new UsernameNotFoundException("Account expired");
            } else {
                return new User(buyer.getUsername(), buyer.getPassword(), Collections.singletonList(new SimpleGrantedAuthority("USER")));
            }
        }
    }

    public static String getFailureReason() {
        return (String)failureReason.get();
    }

    public static void clearFailureReason() {
        failureReason.remove();
    }
}
