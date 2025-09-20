/*
 * Decompiled with CFR 0.152.
 * 
 * Could not load the following classes:
 *  indi.wookat.quickgrab.config.SecurityConfig
 *  indi.wookat.quickgrab.service.CustomUserDetailsService
 *  org.springframework.context.annotation.Bean
 *  org.springframework.context.annotation.Configuration
 *  org.springframework.security.authentication.AuthenticationManager
 *  org.springframework.security.config.annotation.authentication.configuration.AuthenticationConfiguration
 *  org.springframework.security.config.annotation.web.builders.HttpSecurity
 *  org.springframework.security.config.annotation.web.configuration.EnableWebSecurity
 *  org.springframework.security.config.annotation.web.configurers.AuthorizeHttpRequestsConfigurer$AuthorizedUrl
 *  org.springframework.security.config.annotation.web.configurers.FormLoginConfigurer
 *  org.springframework.security.core.userdetails.UserDetailsService
 *  org.springframework.security.crypto.password.NoOpPasswordEncoder
 *  org.springframework.security.crypto.password.PasswordEncoder
 *  org.springframework.security.web.SecurityFilterChain
 *  org.springframework.security.web.authentication.AuthenticationFailureHandler
 *  org.springframework.security.web.authentication.AuthenticationSuccessHandler
 */
package indi.wookat.quickgrab.config;

import indi.wookat.quickgrab.service.CustomUserDetailsService;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.security.authentication.AuthenticationManager;
import org.springframework.security.config.annotation.authentication.configuration.AuthenticationConfiguration;
import org.springframework.security.config.annotation.web.builders.HttpSecurity;
import org.springframework.security.config.annotation.web.configuration.EnableWebSecurity;
import org.springframework.security.config.annotation.web.configurers.AuthorizeHttpRequestsConfigurer;
import org.springframework.security.config.annotation.web.configurers.FormLoginConfigurer;
import org.springframework.security.core.userdetails.UserDetailsService;
import org.springframework.security.crypto.password.NoOpPasswordEncoder;
import org.springframework.security.crypto.password.PasswordEncoder;
import org.springframework.security.web.SecurityFilterChain;
import org.springframework.security.web.authentication.AuthenticationFailureHandler;
import org.springframework.security.web.authentication.AuthenticationSuccessHandler;

@Configuration
@EnableWebSecurity
public class SecurityConfig {
    private final UserDetailsService userDetailsService;

    public SecurityConfig(UserDetailsService userDetailsService) {
        this.userDetailsService = userDetailsService;
    }

    @Bean
    public SecurityFilterChain securityFilterChain(HttpSecurity http) throws Exception {
        http.authorizeHttpRequests(requests -> ((AuthorizeHttpRequestsConfigurer.AuthorizedUrl)((AuthorizeHttpRequestsConfigurer.AuthorizedUrl)((AuthorizeHttpRequestsConfigurer.AuthorizedUrl)requests.requestMatchers(new String[]{"/login.html", "/api/login", "/css/**", "/js/**", "/images/**"})).permitAll().requestMatchers(new String[]{"/api/**"})).authenticated().anyRequest()).authenticated()).formLogin(form -> ((FormLoginConfigurer)((FormLoginConfigurer)((FormLoginConfigurer)form.loginPage("/login.html").loginProcessingUrl("/api/login")).usernameParameter("username").passwordParameter("password").successHandler(this.successHandler())).failureHandler(this.failureHandler())).permitAll()).rememberMe(rememberMe -> rememberMe.key("uniqueAndSecret").tokenValiditySeconds(1296000).userDetailsService(this.userDetailsService)).logout(logout -> logout.logoutUrl("/api/logout").logoutSuccessUrl("/login.html")).csrf().disable();
        return (SecurityFilterChain)http.build();
    }

    @Bean
    public PasswordEncoder passwordEncoder() {
        return NoOpPasswordEncoder.getInstance();
    }

    @Bean
    public AuthenticationManager authenticationManager(AuthenticationConfiguration authenticationConfiguration) throws Exception {
        return authenticationConfiguration.getAuthenticationManager();
    }

    @Bean
    public AuthenticationSuccessHandler successHandler() {
        return (request, response, authentication) -> {
            response.setStatus(200);
            response.getWriter().write("{\"status\": \"success\", \"message\": \"Login successful\"}");
            CustomUserDetailsService.clearFailureReason();
        };
    }

    @Bean
    public AuthenticationFailureHandler failureHandler() {
        return (request, response, exception) -> {
            response.setStatus(401);
            response.setContentType("application/json;charset=UTF-8");
            response.setCharacterEncoding("UTF-8");
            String failureReason = CustomUserDetailsService.getFailureReason();
            if (failureReason != null) {
                response.getWriter().write("{\"status\": \"error\", \"message\": \"" + failureReason + "\"}");
            } else {
                response.getWriter().write("{\"status\": \"error\", \"message\": \"\u7528\u6237\u540d\u6216\u5bc6\u7801\u9519\u8bef\"}");
            }
            CustomUserDetailsService.clearFailureReason();
        };
    }
}

