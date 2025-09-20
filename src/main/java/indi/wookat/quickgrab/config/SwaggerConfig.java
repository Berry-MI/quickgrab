/*
 * Decompiled with CFR 0.152.
 * 
 * Could not load the following classes:
 *  indi.wookat.quickgrab.config.SwaggerConfig
 *  io.swagger.v3.oas.models.ExternalDocumentation
 *  io.swagger.v3.oas.models.OpenAPI
 *  io.swagger.v3.oas.models.info.Info
 *  org.springframework.context.annotation.Bean
 *  org.springframework.context.annotation.Configuration
 */
package indi.wookat.quickgrab.config;

import io.swagger.v3.oas.models.ExternalDocumentation;
import io.swagger.v3.oas.models.OpenAPI;
import io.swagger.v3.oas.models.info.Info;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

@Configuration
public class SwaggerConfig {
    @Bean
    public OpenAPI openAPI() {
        return new OpenAPI().info(new Info().title("\u63a5\u53e3\u6587\u6863\u6807\u9898").description("SpringBoot3 \u96c6\u6210 Swagger3\u63a5\u53e3\u6587\u6863").version("v1")).externalDocs(new ExternalDocumentation().description("\u9879\u76eeAPI\u6587\u6863").url("/"));
    }
}

