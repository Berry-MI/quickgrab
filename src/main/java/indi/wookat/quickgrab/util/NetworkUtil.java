/*
 * Decompiled with CFR 0.152.
 * 
 * Could not load the following classes:
 *  com.fasterxml.jackson.core.JsonProcessingException
 *  com.fasterxml.jackson.databind.JsonNode
 *  com.fasterxml.jackson.databind.ObjectMapper
 *  com.fasterxml.jackson.databind.node.ObjectNode
 *  indi.wookat.quickgrab.entity.Requests
 *  indi.wookat.quickgrab.util.CookieConverter
 *  indi.wookat.quickgrab.util.FindUtil
 *  indi.wookat.quickgrab.util.NetworkUtil
 *  indi.wookat.quickgrab.util.RetryUtil
 *  org.jsoup.Jsoup
 *  org.jsoup.nodes.Document
 *  org.jsoup.nodes.Element
 *  org.slf4j.Logger
 *  org.slf4j.LoggerFactory
 *  org.springframework.stereotype.Component
 *  org.springframework.web.bind.annotation.RequestBody
 */
package indi.wookat.quickgrab.util;

import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.node.ObjectNode;
import indi.wookat.quickgrab.entity.Requests;
import indi.wookat.quickgrab.util.CookieConverter;
import indi.wookat.quickgrab.util.FindUtil;
import indi.wookat.quickgrab.util.RetryUtil;
import java.net.InetAddress;
import java.net.URI;
import java.net.URLEncoder;
import java.net.UnknownHostException;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.nio.charset.StandardCharsets;
import java.time.Duration;
import java.util.ArrayList;
import java.util.Objects;
import java.util.Random;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.Executors;
import org.jsoup.Jsoup;
import org.jsoup.nodes.Document;
import org.jsoup.nodes.Element;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Component;
import org.springframework.web.bind.annotation.RequestBody;

/*
 * Exception performing whole class analysis ignored.
 */
@Component
public class NetworkUtil {
    public static final HttpClient httpClient = HttpClient.newBuilder().connectTimeout(Duration.ofSeconds(15L)).version(HttpClient.Version.HTTP_2).followRedirects(HttpClient.Redirect.NORMAL).executor(Executors.newFixedThreadPool(100)).build();
    private static final ObjectMapper objectMapper = new ObjectMapper();
    private static final Logger logger = LoggerFactory.getLogger(NetworkUtil.class);
    static int index = 0;

    public static String getNickName(String cookies) {
        try {
            String baseUrl = "https://thor.weidian.com/ark/feed.getFeed/1.0?param=";
            String queryParam = "{}";
            String encodedQueryParam = URLEncoder.encode(queryParam, StandardCharsets.UTF_8);
            String fullUrl = baseUrl + encodedQueryParam;
            HttpClient client = HttpClient.newHttpClient();
            HttpRequest request = HttpRequest.newBuilder().uri(new URI(fullUrl)).GET().header("Content-Type", "application/x-www-form-urlencoded;charset=UTF-8").header("Cookie", cookies).header("Referer", "https://weidian.com/").header("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/108.0.0.0 Safari/537.36 Edg/108.0.1462.76").build();
            HttpResponse<String> response = client.send(request, HttpResponse.BodyHandlers.ofString());
            ObjectMapper mapper = new ObjectMapper();
            JsonNode responseBody = mapper.readTree(response.body());
            if (responseBody.get("status").get("code").intValue() == 0) {
                String Username = FindUtil.findValueByKey((JsonNode)responseBody, (String)"userNickName");
                return Username;
            }
        }
        catch (Exception e) {
            e.printStackTrace();
        }
        return null;
    }

    public static JsonNode getUserInfo(String cookies) {
        try {
            String baseUrl = "https://thor.weidian.com/udccore/udc.user.getUserInfoById/1.0?param=";
            String queryParam = "{}";
            String encodedQueryParam = URLEncoder.encode(queryParam, StandardCharsets.UTF_8);
            String fullUrl = baseUrl + encodedQueryParam;
            HttpClient client = HttpClient.newHttpClient();
            HttpRequest request = HttpRequest.newBuilder().uri(new URI(fullUrl)).GET().header("Content-Type", "application/x-www-form-urlencoded;charset=UTF-8").header("Cookie", cookies).header("Referer", "https://weidian.com/").header("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/108.0.0.0 Safari/537.36 Edg/108.0.1462.76").build();
            HttpResponse<String> response = client.send(request, HttpResponse.BodyHandlers.ofString());
            JsonNode rootNode = objectMapper.readTree(response.body());
            if (rootNode.get("status").get("code").asInt() == 0 && rootNode.get("status").get("message").asText().equals("OK")) {
                JsonNode resultNode = rootNode.get("result");
                JsonNode basicNode = resultNode.get("basic");
                JsonNode phoneNode = resultNode.get("phone");
                JsonNode wechatNode = resultNode.get("wechat");
                ObjectNode userInfoNode = objectMapper.createObjectNode();
                userInfoNode.put("nickName", basicNode.has("nickName") && basicNode.get("nickName").isTextual() ? basicNode.get("nickName").asText() : "\u672a\u77e5");
                userInfoNode.put("telephone", phoneNode != null && phoneNode.has("telephone") && phoneNode.get("telephone").isTextual() ? phoneNode.get("telephone").asText() : "\u672a\u77e5");
                userInfoNode.put("sex", wechatNode != null && wechatNode.has("sex") && wechatNode.get("sex").isTextual() ? wechatNode.get("sex").asText() : "\u672a\u77e5");
                userInfoNode.put("province", wechatNode != null && wechatNode.has("province") && wechatNode.get("province").isTextual() ? wechatNode.get("province").asText() : "\u672a\u77e5");
                userInfoNode.put("headImage", basicNode.has("headImage") && basicNode.get("headImage").isTextual() ? basicNode.get("headImage").asText() : "");
                return userInfoNode;
            }
            return objectMapper.createObjectNode();
        }
        catch (Exception e) {
            e.printStackTrace();
            return null;
        }
    }

    public static JsonNode getItemInfo(String itemId, String cookie) {
        try {
            String baseUrl = "https://thor.weidian.com/detailmjb/getItemInfo/1.4";
            String timestamp = String.valueOf(System.currentTimeMillis());
            String context = CookieConverter.convertCookieToJson((String)cookie);
            String param = String.format("{\"adsk\":\"\",\"itemId\":\"%s\"}", itemId);
            String fullUrl = String.format("%s?timestamp=%s&context=%s&param=%s", baseUrl, timestamp, URLEncoder.encode(context, StandardCharsets.UTF_8), URLEncoder.encode(param, StandardCharsets.UTF_8));
            HttpRequest httpRequest = HttpRequest.newBuilder().uri(new URI(fullUrl)).header("Content-Type", "application/x-www-form-urlencoded;charset=UTF-8").header("Referer", "https://android.weidian.com").header("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/108.0.0.0 Safari/537.36 Edg/108.0.1462.76").GET().build();
            HttpResponse<String> response = httpClient.send(httpRequest, HttpResponse.BodyHandlers.ofString());
            JsonNode rootNode = objectMapper.readTree(response.body());
            System.out.println(rootNode);
            if (Objects.equals(rootNode.get("status").get("message").asText(), "OK")) {
                logger.info("\u83b7\u53d6\u5546\u54c1\u4fe1\u606f\u6210\u529f: {}", (Object)itemId);
                return rootNode.get("result").get("defaultModel");
            }
            logger.warn("\u83b7\u53d6\u5546\u54c1\u4fe1\u606f\u5931\u8d25: {}", (Object)rootNode.get("status").get("message").asText());
            return null;
        }
        catch (Exception e) {
            logger.error("\u83b7\u53d6\u5546\u54c1\u4fe1\u606f\u5f02\u5e38", (Throwable)e);
            return null;
        }
    }

    public static JsonNode getOrderDetail(String orderId, String cookies) {
        try {
            String baseUrl = "https://thor.weidian.com/tradeview/buyer.getOrderDetailForApp/1.0?param=";
            String queryParam = String.format("{\"orderId\":\"%s\"}", orderId);
            String encodedQueryParam = URLEncoder.encode(queryParam, StandardCharsets.UTF_8);
            String fullUrl = baseUrl + encodedQueryParam;
            HttpRequest httpRequest = HttpRequest.newBuilder().uri(new URI(fullUrl)).header("Cookie", cookies).header("Content-Type", "application/x-www-form-urlencoded;charset=UTF-8").header("Referer", "https://android.weidian.com").header("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/108.0.0.0 Safari/537.36 Edg/108.0.1462.76").GET().build();
            HttpResponse<String> response = httpClient.send(httpRequest, HttpResponse.BodyHandlers.ofString());
            JsonNode rootNode = objectMapper.readTree(response.body());
            if (Objects.equals(rootNode.get("order").get("status").get("message").asText(), "OK")) {
                logger.info("getOrderDetail: \u6210\u529f\uff0c\u83b7\u53d6\u8ba2\u5355\u4fe1\u606f" + orderId);
                return rootNode.get("order").get("result");
            }
            logger.info("getOrderDetail: \u9519\u8bef, " + rootNode.get("order").get("status").get("message").asText());
            return null;
        }
        catch (Exception e) {
            e.printStackTrace();
            return null;
        }
    }

    public static JsonNode getAddOrderData(@RequestBody Requests request) {
        return (JsonNode)RetryUtil.retryWithExponentialBackoff(() -> NetworkUtil.tryGetAddOrderData((Requests)request), result -> result != null, (int)3, (long)300L, (int)20, (String)"getAddOrderData");
    }

    private static JsonNode tryGetAddOrderData(@RequestBody Requests request) {
        try {
            HttpRequest httpRequest = HttpRequest.newBuilder().uri(new URI(request.getLink())).header("Content-Type", "application/x-www-form-urlencoded;charset=UTF-8").header("Cookie", request.getCookies()).header("Referer", "https://weidian.com/").header("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/108.0.0.0 Safari/537.36 Edg/108.0.1462.76").GET().build();
            HttpResponse<String> response = httpClient.send(httpRequest, HttpResponse.BodyHandlers.ofString());
            JsonNode responseBody = NetworkUtil.getDataObject((String)response.body());
            if (Objects.equals(responseBody.get("order").get("status").get("message").asText(), "OK")) {
                if (responseBody.has("confirmOrderParam")) {
                    ((ObjectNode)responseBody.get("order").get("result")).set("confirmOrderParam", responseBody.get("confirmOrderParam"));
                }
                return responseBody.get("order").get("result");
            }
            return null;
        }
        catch (Exception e) {
            logger.error("tryGetAddOrderData: \u53d1\u751f\u5f02\u5e38", (Throwable)e);
            return null;
        }
    }

    public static JsonNode getItemData(String itemID) {
        try {
            HttpClient httpClient = HttpClient.newBuilder().followRedirects(HttpClient.Redirect.NORMAL).build();
            HttpRequest httpRequest = HttpRequest.newBuilder().uri(new URI("https://weidian.com/item.html?itemID=" + itemID)).header("Content-Type", "application/x-www-form-urlencoded;charset=UTF-8").header("Referer", "https://weidian.com/").header("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/108.0.0.0 Safari/537.36 Edg/108.0.1462.76").GET().build();
            HttpResponse<String> response = httpClient.send(httpRequest, HttpResponse.BodyHandlers.ofString());
            JsonNode responseBody = NetworkUtil.getDataObject((String)response.body());
            if (Objects.equals(responseBody.get("status").get("message").asText(), "OK")) {
                return responseBody.get("result").get("default_model");
            }
            return null;
        }
        catch (Exception e) {
            e.printStackTrace();
            return null;
        }
    }

    public static JsonNode getNewOrderDetailData(@RequestBody Requests request, String orderId) {
        try {
            HttpClient httpClient = HttpClient.newBuilder().followRedirects(HttpClient.Redirect.NORMAL).build();
            HttpRequest httpRequest = HttpRequest.newBuilder().uri(new URI("https://weidian.com/user/order-new/detail.php?oid=" + orderId)).header("Content-Type", "application/x-www-form-urlencoded;charset=UTF-8").header("Cookie", request.getCookies()).header("Referer", "https://weidian.com/").header("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/108.0.0.0 Safari/537.36 Edg/108.0.1462.76").GET().build();
            HttpResponse<String> response = httpClient.send(httpRequest, HttpResponse.BodyHandlers.ofString());
            JsonNode responseBody = NetworkUtil.getDataObject((String)response.body());
            return responseBody;
        }
        catch (Exception e) {
            e.printStackTrace();
            return null;
        }
    }

    public static JsonNode getDataObject(String htmlResponse) throws JsonProcessingException {
        Document doc = Jsoup.parse((String)htmlResponse);
        Element scriptTag = doc.selectFirst("#__rocker-render-inject__");
        if (scriptTag != null) {
            String dataObjAttr = scriptTag.attr("data-obj");
            ObjectMapper objectMapper = new ObjectMapper();
            JsonNode jsonNode = objectMapper.readTree(dataObjAttr);
            return jsonNode;
        }
        System.out.println("getItemData: Script tag with id '__rocker-render-inject__' not found.");
        return null;
    }

    public static JsonNode sendExhibitSpaceJsonRequest(@RequestBody Requests request) {
        try {
            String domain = "thor.weidian.com";
            String param = "{\"exhibitCode\":\"h5_activity\",\"pageSize\":10}";
            String jsonParam = URLEncoder.encode(param, StandardCharsets.UTF_8);
            HttpRequest httpRequest = HttpRequest.newBuilder().uri(new URI("https://" + domain + "/poseidon/exhibit.spaceJson/1.0?param=" + jsonParam)).header("Content-Type", "application/x-www-form-urlencoded;charset=UTF-8").header("Cookie", request.getCookies()).header("Referer", "https://weidian.com/").header("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/108.0.0.0 Safari/537.36 Edg/108.0.1462.76").GET().build();
            long startTime = System.currentTimeMillis();
            HttpResponse<String> response = httpClient.send(httpRequest, HttpResponse.BodyHandlers.ofString());
            long endTime = System.currentTimeMillis();
            JsonNode result = objectMapper.readTree(response.body());
            long serverTime = result.get("result").get("currentTime").asLong();
            long takeTime = endTime - startTime;
            long networkDelay = serverTime - (endTime + startTime) / 2L;
            long ser2staTime = serverTime - startTime;
            long end2serTime = endTime - serverTime;
            JsonNode resultNode = result.get("result");
            ((ObjectNode)resultNode).put("takeTime", takeTime);
            ((ObjectNode)resultNode).put("networkDelay", networkDelay);
            ((ObjectNode)resultNode).put("startTime", startTime);
            ((ObjectNode)resultNode).put("endTime", endTime);
            ((ObjectNode)resultNode).put("serverTime", serverTime);
            ((ObjectNode)resultNode).put("ser2staTime", ser2staTime);
            ((ObjectNode)resultNode).put("end2serTime", end2serTime);
            return resultNode;
        }
        catch (Exception e) {
            e.printStackTrace();
            return null;
        }
    }

    public static JsonNode sendReConfirmOrderRequest(@RequestBody Requests request) {
        try {
            String domain = "thor.weidian.com";
            String jsonParam = URLEncoder.encode(request.getOrderParameters(), StandardCharsets.UTF_8);
            HttpRequest httpRequest = HttpRequest.newBuilder().uri(new URI("https://" + domain + "/vbuy/ReConfirmOrder/1.0")).header("Content-Type", "application/x-www-form-urlencoded;charset=UTF-8").header("Cookie", request.getCookies()).header("Referer", "https://weidian.com/").header("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/108.0.0.0 Safari/537.36 Edg/108.0.1462.76").POST(HttpRequest.BodyPublishers.ofString("param=" + jsonParam)).build();
            long startTime = System.currentTimeMillis();
            HttpResponse<String> response = httpClient.send(httpRequest, HttpResponse.BodyHandlers.ofString());
            long endTime = System.currentTimeMillis();
            JsonNode result = objectMapper.readTree(response.body());
            if (result.get("status").get("code").asInt() != 0) {
                logger.info("\u786e\u8ba4\u8ba2\u5355\u5931\u8d25\uff1a{}", (Object)result.get("status").get("message").asText());
                return null;
            }
            long serverTime = result.get("result").get("access_time").asLong();
            long takeTime = endTime - startTime;
            long networkDelay = serverTime - (endTime + startTime) / 2L;
            long ser2staTime = serverTime - startTime;
            long end2serTime = endTime - serverTime;
            logger.info("sendReConfirmOrderRequest\t: \u57df\u540d: {}\t, \u7528\u65f6: {}ms, \u7f51\u7edc\u5ef6\u8fdf: {}ms, \u5f00\u59cb\u65f6\u95f4: {}, \u670d\u52a1\u5668\u65f6\u95f4: {}, \u7ed3\u675f\u65f6\u95f4: {}, \u670d\u52a1\u5668\u65f6\u95f4-\u5f00\u59cb\u65f6\u95f4: {}ms, \u7ed3\u675f\u65f6\u95f4-\u670d\u52a1\u5668\u65f6\u95f4: {}ms", new Object[]{domain, takeTime, networkDelay, startTime, serverTime, endTime, ser2staTime, end2serTime});
            JsonNode resultNode = result.get("result");
            ((ObjectNode)resultNode).put("takeTime", takeTime);
            ((ObjectNode)resultNode).put("networkDelay", networkDelay);
            ((ObjectNode)resultNode).put("startTime", startTime);
            ((ObjectNode)resultNode).put("endTime", endTime);
            ((ObjectNode)resultNode).put("serverTime", serverTime);
            ((ObjectNode)resultNode).put("ser2staTime", ser2staTime);
            ((ObjectNode)resultNode).put("end2serTime", end2serTime);
            return resultNode;
        }
        catch (Exception e) {
            e.printStackTrace();
            return null;
        }
    }

    public static JsonNode sendCreateOrderRequest(@RequestBody Requests request) {
        try {
            String domain = "thor.weidian.com";
            String jsonParam = URLEncoder.encode(request.getOrderParameters(), StandardCharsets.UTF_8);
            HttpRequest httpRequest = HttpRequest.newBuilder().uri(new URI("https://thor.weidian.com/vbuy/CreateOrder/1.0")).header("Content-Type", "application/x-www-form-urlencoded;charset=UTF-8").header("Cookie", request.getCookies()).header("Referer", "https://weidian.com/").header("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/108.0.0.0 Safari/537.36 Edg/108.0.1462.76").POST(HttpRequest.BodyPublishers.ofString("param=" + jsonParam)).build();
            long startTime = System.currentTimeMillis();
            HttpResponse<String> response = httpClient.send(httpRequest, HttpResponse.BodyHandlers.ofString());
            long endTime = System.currentTimeMillis();
            JsonNode result = objectMapper.readTree(response.body());
            System.out.println(result);
            long takeTime = endTime - startTime;
            logger.info("sendCreateOrderRequest\t\t: \u5f00\u59cb\u65f6\u95f4: {}, \u7ed3\u675f\u65f6\u95f4: {}, \u7528\u65f6: {}ms", new Object[]{startTime, endTime, takeTime});
            ObjectNode resultNode = objectMapper.createObjectNode();
            resultNode.put("takeTime", takeTime);
            resultNode.put("startTime", startTime);
            resultNode.put("endTime", endTime);
            return resultNode;
        }
        catch (Exception e) {
            e.printStackTrace();
            return null;
        }
    }

    public static CompletableFuture<JsonNode> sendAsyncExhibitSpaceJsonRequest(@RequestBody Requests request) {
        return CompletableFuture.supplyAsync(() -> {
            try {
                String domain = "thor.weidian.com";
                String param = "{\"exhibitCode\":\"h5_activity\",\"pageSize\":10}";
                String jsonParam = URLEncoder.encode(param, StandardCharsets.UTF_8);
                HttpRequest httpRequest = HttpRequest.newBuilder().uri(new URI("https://" + domain + "/poseidon/exhibit.spaceJson/1.0?param=" + jsonParam)).header("Content-Type", "application/x-www-form-urlencoded;charset=UTF-8").header("Cookie", request.getCookies()).header("Referer", "https://weidian.com/").header("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/108.0.0.0 Safari/537.36 Edg/108.0.1462.76").GET().build();
                long startTime = System.currentTimeMillis();
                HttpResponse<String> response = httpClient.send(httpRequest, HttpResponse.BodyHandlers.ofString());
                long endTime = System.currentTimeMillis();
                JsonNode result = objectMapper.readTree(response.body());
                long serverTime = result.get("result").get("currentTime").asLong();
                long takeTime = endTime - startTime;
                long networkDelay = serverTime - (endTime + startTime) / 2L;
                long ser2staTime = serverTime - startTime;
                long end2serTime = endTime - serverTime;
                JsonNode resultNode = result.get("result");
                ((ObjectNode)resultNode).put("takeTime", takeTime);
                ((ObjectNode)resultNode).put("networkDelay", networkDelay);
                ((ObjectNode)resultNode).put("startTime", startTime);
                ((ObjectNode)resultNode).put("endTime", endTime);
                ((ObjectNode)resultNode).put("serverTime", serverTime);
                ((ObjectNode)resultNode).put("ser2staTime", ser2staTime);
                ((ObjectNode)resultNode).put("end2serTime", end2serTime);
                return resultNode;
            }
            catch (Exception e) {
                e.printStackTrace();
                return null;
            }
        });
    }

    public static CompletableFuture<JsonNode> sendNotCookieCreateOrderRequest(@RequestBody Requests request) {
        return CompletableFuture.supplyAsync(() -> {
            try {
                String jsonParam = URLEncoder.encode(request.getOrderParameters(), StandardCharsets.UTF_8);
                HttpRequest httpRequest = HttpRequest.newBuilder().uri(new URI("https://thor.weidian.com/vbuy/CreateOrder/1.0")).header("Content-Type", "application/x-www-form-urlencoded;charset=UTF-8").header("Referer", "https://weidian.com/").header("Cookie", request.getCookies()).header("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/108.0.0.0 Safari/537.36 Edg/108.0.1462.76").POST(HttpRequest.BodyPublishers.ofString("param=" + jsonParam)).build();
                long startTime = System.currentTimeMillis();
                HttpResponse<String> response = httpClient.send(httpRequest, HttpResponse.BodyHandlers.ofString());
                long endTime = System.currentTimeMillis();
                JsonNode result = objectMapper.readTree(response.body());
                long takeTime = endTime - startTime;
                logger.info("sendNotCookieCreateOrderRequest\t\t: \u5f00\u59cb\u65f6\u95f4: {}, \u7ed3\u675f\u65f6\u95f4: {}, \u7528\u65f6: {}ms, \u8fd4\u56de\uff1a{}", new Object[]{startTime, endTime, takeTime, result});
                ObjectNode resultNode = objectMapper.createObjectNode();
                resultNode.put("takeTime", takeTime);
                resultNode.put("startTime", startTime);
                resultNode.put("endTime", endTime);
                return resultNode;
            }
            catch (Exception e) {
                e.printStackTrace();
                return null;
            }
        });
    }

    public static CompletableFuture<JsonNode> sendPreCreateOrderRequest(@RequestBody Requests request) {
        return CompletableFuture.supplyAsync(() -> {
            try {
                String jsonParam = URLEncoder.encode(request.getOrderParameters(), StandardCharsets.UTF_8);
                HttpRequest httpRequest = HttpRequest.newBuilder().uri(new URI("https://thor.weidian.com/vbuy/CreateOrder/1.0")).header("Content-Type", "application/x-www-form-urlencoded;charset=UTF-8").header("Referer", "https://weidian.com/").header("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/108.0.0.0 Safari/537.36 Edg/108.0.1462.76").POST(HttpRequest.BodyPublishers.ofString("param=" + jsonParam)).build();
                long startTime = System.currentTimeMillis();
                HttpResponse<String> response = httpClient.send(httpRequest, HttpResponse.BodyHandlers.ofString());
                long endTime = System.currentTimeMillis();
                JsonNode result = objectMapper.readTree(response.body());
                long takeTime = endTime - startTime;
                logger.info("sendPreCreateOrderRequest\t\t: \u5f00\u59cb\u65f6\u95f4: {}, \u7ed3\u675f\u65f6\u95f4: {}, \u7528\u65f6: {}ms, \u8fd4\u56de\uff1a{}", new Object[]{startTime, endTime, takeTime, result});
                ObjectNode resultNode = objectMapper.createObjectNode();
                resultNode.put("takeTime", takeTime);
                resultNode.put("startTime", startTime);
                resultNode.put("endTime", endTime);
                return resultNode;
            }
            catch (Exception e) {
                e.printStackTrace();
                return null;
            }
        });
    }

    public static JsonNode getSingleGoodsInventory(JsonNode itemData) {
        try {
            String domain = NetworkUtil.getRandomDomain();
            String param = "{\"itemId\":\"" + itemData.get("item_id").asText() + "\",\"source\":\"h5\",\"skuId\":\"" + itemData.get("item_sku_id").asText() + "\",\"count\":" + itemData.get("quantity").asInt() + "}";
            String encodedParam = URLEncoder.encode(param, StandardCharsets.UTF_8.toString());
            HttpClient httpClient = HttpClient.newBuilder().followRedirects(HttpClient.Redirect.NORMAL).build();
            HttpRequest request = HttpRequest.newBuilder().uri(new URI("https://" + domain + "/vcart/addCart/2.0?param=" + encodedParam)).header("Content-Type", "application/x-www-form-urlencoded;charset=UTF-8").header("Referer", "https://weidian.com/").header("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/108.0.0.0 Safari/537.36 Edg/108.0.1462.76").GET().build();
            HttpResponse<String> response = httpClient.send(request, HttpResponse.BodyHandlers.ofString());
            return objectMapper.readTree(response.body());
        }
        catch (Exception e) {
            e.printStackTrace();
            return null;
        }
    }

    public static JsonNode getItemSkuInfoByItemId(String itemId) {
        try {
            String domain = NetworkUtil.getRandomDomain();
            String param = "{\"itemId\":\"" + itemId + "\"}";
            String encodedParam = URLEncoder.encode(param, StandardCharsets.UTF_8.toString());
            HttpClient httpClient = HttpClient.newBuilder().followRedirects(HttpClient.Redirect.NORMAL).build();
            HttpRequest request = HttpRequest.newBuilder().uri(new URI("https://" + domain + "/detailmjb/getItemSkuInfo/1.0?param=" + encodedParam)).header("Content-Type", "application/x-www-form-urlencoded;charset=UTF-8").header("Referer", "https://weidian.com/").header("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/108.0.0.0 Safari/537.36 Edg/108.0.1462.76").GET().build();
            HttpResponse<String> response = httpClient.send(request, HttpResponse.BodyHandlers.ofString());
            return objectMapper.readTree(response.body());
        }
        catch (Exception e) {
            e.printStackTrace();
            return null;
        }
    }

    public static JsonNode getShopItemList(String shopId) throws Exception {
        String domain = NetworkUtil.getRandomDomain();
        String baseUrl = "https://" + domain + "/decorate/shopDetail.tab.getItemList/1.0";
        String param = "{\"shopId\":\"%s\",\"tabId\":3,\"sortOrder\":\"desc\",\"offset\":0,\"limit\":20,\"from\":\"h5\",\"showItemTag\":false}";
        String formattedParam = String.format(param, shopId);
        String encodedParam = URLEncoder.encode(formattedParam, StandardCharsets.UTF_8.toString());
        String fixedUrl = baseUrl + "?param=" + encodedParam;
        HttpClient client = HttpClient.newBuilder().connectTimeout(Duration.ofSeconds(10L)).build();
        HttpRequest request = HttpRequest.newBuilder().uri(URI.create(fixedUrl)).header("Content-Type", "application/x-www-form-urlencoded;charset=UTF-8").header("Referer", "https://weidian.com/").header("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/108.0.0.0 Safari/537.36 Edg/108.0.1462.76").GET().build();
        HttpResponse<String> response = client.send(request, HttpResponse.BodyHandlers.ofString());
        return objectMapper.readTree(response.body());
    }

    public static JsonNode getItemSkuInfo(String itemId) throws Exception {
        String domain = NetworkUtil.getRandomDomain();
        String baseUrl = "https://" + domain + "/detail/getItemSkuInfo/1.0";
        String param = "{\"itemId\":\"%s\"}";
        String formattedParam = String.format(param, itemId);
        String encodedParam = URLEncoder.encode(formattedParam, StandardCharsets.UTF_8.toString());
        String fixedUrl = baseUrl + "?param=" + encodedParam;
        HttpClient client = HttpClient.newBuilder().connectTimeout(Duration.ofSeconds(10L)).build();
        HttpRequest request = HttpRequest.newBuilder().uri(URI.create(fixedUrl)).header("Content-Type", "application/json;charset=UTF-8").GET().build();
        HttpResponse<String> response = client.send(request, HttpResponse.BodyHandlers.ofString());
        return objectMapper.readTree(response.body());
    }

    public static String getRandomDomain() {
        ArrayList<String> domains = new ArrayList<String>();
        domains.add("thor.weidian.com");
        domains.add("thor.mitao.cn");
        domains.add("thor.kou6ai.cn");
        domains.add("thor.bibikan.cn");
        domains.add("thor.koudai.com");
        for (int i = 1; i <= 10; ++i) {
            domains.add(String.format("thor.youshop%02d.com", i));
        }
        for (int attempt = 0; attempt < 3; ++attempt) {
            String domain = (String)domains.get(new Random().nextInt(domains.size()));
            if (NetworkUtil.isDomainResolvable((String)domain)) {
                return domain;
            }
            domains.remove(domain);
            if (!domains.isEmpty()) continue;
            return "thor.weidian.com";
        }
        return "thor.weidian.com";
    }

    private static boolean isDomainResolvable(String domain) {
        try {
            InetAddress.getByName(domain);
            return true;
        }
        catch (UnknownHostException e) {
            logger.warn("\u57df\u540d {} \u65e0\u6cd5\u89e3\u6790: {}", (Object)domain, (Object)e.getMessage());
            return false;
        }
    }

    public static String getSequentialDomain() {
        ArrayList<String> domains = new ArrayList<String>();
        domains.add("thor.mitao.cn");
        domains.add("thor.weidian.com");
        domains.add("thor.kou6ai.cn");
        domains.add("thor.bibikan.cn");
        domains.add("thor.koudai.com");
        for (int i = 1; i <= 10; ++i) {
            domains.add(String.format("thor.youshop%02d.com", i));
        }
        for (int attempt = 0; attempt < 3; ++attempt) {
            String domain = (String)domains.get(index);
            index = (index + 1) % domains.size();
            if (NetworkUtil.isDomainResolvable((String)domain)) {
                return domain;
            }
            domains.remove(domain);
            if (!domains.isEmpty()) continue;
            return "thor.weidian.com";
        }
        return "thor.weidian.com";
    }
}

