//
// Source code recreated from a .class file by IntelliJ IDEA
// (powered by FernFlower decompiler)
//

package indi.wookat.quickgrab.util;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.node.ObjectNode;
import java.util.HashMap;
import java.util.Map;
import java.util.Random;
import java.util.UUID;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class CookieConverter {
    private static final Logger logger = LoggerFactory.getLogger(CookieConverter.class);
    private static final ObjectMapper objectMapper = new ObjectMapper();
    private static final Random random = new Random();

    public static String convertCookieToJson(String cookieStr) {
        try {
            Map<String, String> cookieMap = parseCookieString(cookieStr);
            ObjectNode jsonNode = objectMapper.createObjectNode();
            jsonNode.put("alt", "0.0");
            jsonNode.put("android_id", generateAndroidId());
            jsonNode.put("app_status", "active");
            jsonNode.put("appid", "com.koudai.weidian.buyer");
            jsonNode.put("appv", generateAppVersion());
            jsonNode.put("brand", generateBrand());
            jsonNode.put("build", generateBuildNumber());
            jsonNode.put("channel", generateChannel());
            jsonNode.put("cuid", generateCuid());
            jsonNode.put("disk_capacity", generateDiskCapacity());
            jsonNode.put("feature", generateFeature());
            jsonNode.put("h", generateScreenHeight());
            jsonNode.put("iccid", "");
            jsonNode.put("imei", "");
            jsonNode.put("imsi", "");
            jsonNode.put("lat", generateLatitude());
            jsonNode.put("lon", generateLongitude());
            jsonNode.put("mac", generateMac());
            jsonNode.put("machine_model", generateMachineModel());
            jsonNode.put("memory", generateMemory());
            jsonNode.put("mid", generateMid());
            jsonNode.put("mobile_station", "0");
            jsonNode.put("net_subtype", "0_");
            jsonNode.put("network", "WIFI");
            jsonNode.put("oaid", generateOaid());
            jsonNode.put("os", generateOsVersion());
            jsonNode.put("platform", "android");
            jsonNode.put("serial_num", "");
            jsonNode.put("w", generateScreenWidth());
            jsonNode.put("wmac", generateWifiMac());
            jsonNode.put("wssid", generateWifiSsid());
            jsonNode.put("is_login", (String)cookieMap.getOrDefault("is_login", "1"));
            jsonNode.put("login_token", (String)cookieMap.getOrDefault("login_token", ""));
            jsonNode.put("duid", (String)cookieMap.getOrDefault("duid", ""));
            jsonNode.put("uid", (String)cookieMap.getOrDefault("uid", ""));
            jsonNode.put("shop_id", (String)cookieMap.getOrDefault("duid", ""));
            jsonNode.put("suid", generateCuid());
            return objectMapper.writeValueAsString(jsonNode);
        } catch (Exception e) {
            logger.error("转换cookie失败", e);
            return null;
        }
    }

    private static Map<String, String> parseCookieString(String cookieStr) {
        Map<String, String> cookieMap = new HashMap();
        if (cookieStr != null && !cookieStr.trim().isEmpty()) {
            String[] cookies = cookieStr.split(";");

            for(String cookie : cookies) {
                String[] parts = cookie.trim().split("=", 2);
                if (parts.length == 2) {
                    cookieMap.put(parts[0].trim(), parts[1].trim());
                }
            }

            return cookieMap;
        } else {
            return cookieMap;
        }
    }

    private static String generateAndroidId() {
        return UUID.randomUUID().toString().replaceAll("-", "").substring(0, 16);
    }

    private static String generateCuid() {
        return UUID.randomUUID().toString().replaceAll("-", "").substring(0, 32);
    }

    private static String generateOaid() {
        return UUID.randomUUID().toString().replaceAll("-", "").substring(0, 16);
    }

    private static String generateAppVersion() {
        return String.format("%d.%d.%d", random.nextInt(10), random.nextInt(20), random.nextInt(100));
    }

    private static String generateBrand() {
        String[] brands = new String[]{"Xiaomi", "HUAWEI", "OPPO", "vivo", "Samsung", "OnePlus"};
        return brands[random.nextInt(brands.length)];
    }

    private static String generateBuildNumber() {
        return String.format("2025%02d%02d%06d", random.nextInt(12) + 1, random.nextInt(28) + 1, random.nextInt(1000000));
    }

    private static String generateChannel() {
        return String.format("100%d", random.nextInt(10));
    }

    private static String generateDiskCapacity() {
        int total = 32 + random.nextInt(129);
        int used = random.nextInt(total);
        return String.format("%.2fGB/%.2fGB", (float)used, (float)total);
    }

    private static String generateFeature() {
        String[] features = new String[]{"E|F", "H|F", "P|F", "R|T", "W|F"};
        return String.join(",", features);
    }

    private static String generateScreenHeight() {
        int[] heights = new int[]{1920, 2160, 2400, 2560};
        return String.valueOf(heights[random.nextInt(heights.length)]);
    }

    private static String generateScreenWidth() {
        int[] widths = new int[]{1080, 1440, 1600};
        return String.valueOf(widths[random.nextInt(widths.length)]);
    }

    private static String generateLatitude() {
        return String.format("%.6f", (double)18.0F + random.nextDouble() * (double)20.0F);
    }

    private static String generateLongitude() {
        return String.format("%.6f", (double)73.0F + random.nextDouble() * (double)50.0F);
    }

    private static String generateMac() {
        return String.format("%02x:%02x:%02x:%02x:%02x:%02x", random.nextInt(256), random.nextInt(256), random.nextInt(256), random.nextInt(256), random.nextInt(256), random.nextInt(256));
    }

    private static String generateMachineModel() {
        String[] models = new String[]{"aarch64", "arm64-v8a", "armeabi-v7a", "x86_64"};
        return models[random.nextInt(models.length)];
    }

    private static String generateMemory() {
        int total = 4 + random.nextInt(9);
        int used = random.nextInt(total);
        return String.format("%dM/%dM", used * 1024, total * 1024);
    }

    private static String generateMid() {
        String[] models = new String[]{"MI_6", "MI_8", "MI_9", "MI_10", "MI_11", "MI_12"};
        return models[random.nextInt(models.length)];
    }

    private static String generateOsVersion() {
        return String.valueOf(24 + random.nextInt(10));
    }

    private static String generateWifiMac() {
        return String.format("%02x:%02x:%02x:%02x:%02x:%02x", random.nextInt(256), random.nextInt(256), random.nextInt(256), random.nextInt(256), random.nextInt(256), random.nextInt(256));
    }

    private static String generateWifiSsid() {
        String[] prefixes = new String[]{"JXNUSDI", "CMCC", "ChinaNet", "TP-LINK", "HUAWEI"};
        String[] suffixes = new String[]{"_stu", "_5G", "_guest", "_home", "_office"};
        return String.format("\"%s%s\"", prefixes[random.nextInt(prefixes.length)], suffixes[random.nextInt(suffixes.length)]);
    }
}
