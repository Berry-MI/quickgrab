//
// Source code recreated from a .class file by IntelliJ IDEA
// (powered by FernFlower decompiler)
//

package indi.wookat.quickgrab.service;

import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.node.ObjectNode;
import indi.wookat.quickgrab.entity.Requests;
import indi.wookat.quickgrab.entity.Results;
import indi.wookat.quickgrab.mapper.RequestsMapper;
import indi.wookat.quickgrab.mapper.ResultsMapper;
import indi.wookat.quickgrab.util.CommonUtil;
import indi.wookat.quickgrab.util.FindUtil;
import indi.wookat.quickgrab.util.NetworkUtil;
import indi.wookat.quickgrab.util.RetryUtil;
import jakarta.annotation.Resource;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.math.BigDecimal;
import java.net.URI;
import java.net.URISyntaxException;
import java.net.URL;
import java.net.URLDecoder;
import java.net.URLEncoder;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.net.http.HttpRequest.BodyPublishers;
import java.net.http.HttpResponse.BodyHandlers;
import java.nio.charset.StandardCharsets;
import java.time.Duration;
import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ThreadLocalRandom;
import java.util.concurrent.TimeUnit;
import java.util.stream.Collectors;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.scheduling.annotation.Async;
import org.springframework.stereotype.Service;

@Service
public class GrabService {
    private static final ObjectMapper mapper = new ObjectMapper();
    private static final Logger logger = LoggerFactory.getLogger(GrabService.class);
    private static final ScheduledExecutorService scheduler = Executors.newScheduledThreadPool(100);
    public long adjustedFactor = 10L;
    public long precessingTime = 19L;
    public LocalDateTime updateTime = LocalDateTime.now();
    public LocalDateTime prestartTime = LocalDateTime.now();
    private static final List<String> RETRY_KEYWORDS = Collections.unmodifiableList(Arrays.asList("请稍后再试", "拥挤", "重试", "稍后", "人潮拥挤", "商品尚未开售", "开小差", "系统开小差了", "系统开小差", "啊哦~ 人潮拥挤，请稍后重试~"));
    private static final List<String> UPDATE_KEYWORDS = Collections.unmodifiableList(Arrays.asList("确认", "地址", "自提", "应付总额有变动，请再次确认", "商品信息变更，请重新确认", "模板需要收货地址，请联系商家", "店铺信息不能为空", "购买的商品超过限购数", "请先填写收货人地址", "请升级到最新版本后重试", "当前下单商品仅支持到店自提，请重新选择收货方式", "系统开小差，请稍后重试", "自提点地址不能为空"));
    private static final int MAX_CREATE_ORDER_RETRY = 3;
    private static final int MAX_RECONFIRM_RETRY = 3;
    private static final long BASE_RETRY_INTERVAL = 100L;
    public long schedulingTime = computeSchedulingTime();
    @Resource
    private RequestsMapper requestsMapper;
    @Resource
    private ResultsMapper resultsMapper;

    public static long computeSchedulingTime() {
        long startTime = System.nanoTime();
        try {
            return scheduler.schedule(() -> System.nanoTime() - startTime, 0L, TimeUnit.MILLISECONDS)
                    .get(100, TimeUnit.MILLISECONDS) / 1_000_000L;
        } catch (Exception e) {
            logger.debug("计算调度耗时失败，使用默认值: {}", e.getMessage());
            return 2L;
        }
    }

    public static JsonNode createOrder(HttpRequest httpRequest, Requests request) {
        ObjectNode responseBody = mapper.createObjectNode();

        for(int retryCount = 0; retryCount <= MAX_CREATE_ORDER_RETRY; ++retryCount) {
            try {
                HttpResponse<String> response = NetworkUtil.httpClient.send(httpRequest, BodyHandlers.ofString());
                JsonNode parsedBody = mapper.readTree(response.body());
                if (parsedBody instanceof ObjectNode objectNode) {
                    responseBody = objectNode;
                } else {
                    responseBody.removeAll();
                    responseBody.set("raw", parsedBody);
                }

                logger.info("ID：{} 信息：{}", request.getId(), responseBody);
                String message = responseBody.path("status").path("description").asText("");
                int code = responseBody.path("status").path("code").asInt();
                if (code == 0) {
                    CommonUtil.processOrders(responseBody, request);
                    responseBody.put("isSuccess", 1);
                    responseBody.put("isContinue", false);
                    responseBody.put("isUpdate", false);
                } else if (shouldUpdate(message)) {
                    responseBody.put("isSuccess", 2);
                    responseBody.put("isContinue", true);
                    responseBody.put("isUpdate", true);
                } else if (shouldRetry(message)) {
                    responseBody.put("isSuccess", 2);
                    responseBody.put("isContinue", true);
                    responseBody.put("isUpdate", false);
                } else {
                    responseBody.put("isSuccess", 2);
                    responseBody.put("isContinue", false);
                    responseBody.put("isUpdate", false);
                }

                return responseBody;
            } catch (Exception e) {
                if (retryCount >= MAX_CREATE_ORDER_RETRY) {
                    responseBody.put("isSuccess", 3);
                    responseBody.put("isContinue", true);
                    responseBody.put("isUpdate", false);
                    responseBody.put("error", getFirstThreeLinesOfStackTrace(e));
                    logger.error("ID：{} 创建订单失败，已达到最大重试次数: {}", request.getId(), e.getMessage());
                    break;
                }

                logger.error("ID：{} 创建订单失败 (尝试 {}/{}): {}", request.getId(), retryCount + 1, MAX_CREATE_ORDER_RETRY, e.getMessage());
                sleepWithJitter(retryCount + 1, BASE_RETRY_INTERVAL, request.getId(), "创建订单");
            }
        }

        return responseBody;
    }

    public static JsonNode reConfirmOrder(HttpRequest httpRequest) {
        for(int retryCount = 0; retryCount <= MAX_RECONFIRM_RETRY; ++retryCount) {
            try {
                HttpResponse<String> response = NetworkUtil.httpClient.send(httpRequest, BodyHandlers.ofString());
                JsonNode responseBody = mapper.readTree(response.body());
                if (!responseBody.has("result")) {
                    if (retryCount < MAX_RECONFIRM_RETRY) {
                        logger.warn("reConfirmOrder没有result字段，响应内容: {}", responseBody);
                        sleepWithJitter(retryCount + 1, 80L, null, "确认订单");
                        continue;
                    }
                    break;
                }

                return responseBody.get("result");
            } catch (Exception e) {
                if (retryCount >= MAX_RECONFIRM_RETRY) {
                    break;
                }

                logger.error("reConfirmOrder失败 (尝试 {}/{}): {}", retryCount + 1, MAX_RECONFIRM_RETRY, e.getMessage());
                sleepWithJitter(retryCount + 1, 80L, null, "确认订单");
            }
        }

        logger.error("reConfirmOrder达到最大重试次数: {}", MAX_RECONFIRM_RETRY);
        return null;
    }

    private static boolean shouldRetry(String message) {
        return messageContainsAny(message, RETRY_KEYWORDS);
    }

    private static boolean shouldUpdate(String message) {
        return messageContainsAny(message, UPDATE_KEYWORDS);
    }

    private static boolean messageContainsAny(String message, List<String> keywords) {
        if (message == null || message.isEmpty()) {
            return false;
        }

        for (String keyword : keywords) {
            if (message.contains(keyword)) {
                return true;
            }
        }

        return false;
    }

    private static void sleepWithJitter(int attempt, long baseInterval, Integer requestId, String context) {
        long waitTime = Math.max(50L, (long) (Math.pow(2.0D, attempt) * baseInterval));
        long jitter = ThreadLocalRandom.current().nextLong(-waitTime / 5, waitTime / 5 + 1);
        long finalDelay = waitTime + jitter;
        if (requestId != null) {
            logger.info("ID：{} 等待 {}ms 后重试{}", requestId, finalDelay, context == null ? "" : " - " + context);
        } else {
            logger.info("等待 {}ms 后重试{}", finalDelay, context == null ? "" : " - " + context);
        }

        try {
            TimeUnit.MILLISECONDS.sleep(finalDelay);
        } catch (InterruptedException ie) {
            Thread.currentThread().interrupt();
            logger.error("重试等待被中断: {}", ie.getMessage());
        }
    }

    @Async
    public void executeGrab(Requests request) {
        try {
            JsonNode extensionNode = mapper.readTree(request.getExtension());
            boolean quickMode = extensionNode.has("quickMode") && extensionNode.get("quickMode").asBoolean();
            if (!quickMode) {
                try {
                    JsonNode addOrderData = NetworkUtil.getAddOrderData(request);
                    if (addOrderData != null) {
                        JsonNode orderParams = (JsonNode)RetryUtil.retryWithExponentialBackoff(() -> CommonUtil.generateOrderParameters(request, addOrderData, true), (result) -> result != null, 3, 100L, 20, "ID：" + request.getId() + " 生成订单参数");
                        if (orderParams != null) {
                            request.setOrderParameters(orderParams.toString());
                            logger.info("ID：{} 生成订单参数成功", request.getId());
                        } else {
                            logger.error("ID：{} 生成订单参数失败，将使用现有参数继续执行", request.getId());
                        }
                    }
                } catch (Exception e) {
                    logger.error("ID：{} 生成订单参数过程出现严重错误: {}", request.getId(), e.getMessage());
                }
            } else {
                logger.info("ID：{} 使用快速模式，跳过重新生成订单参数", request.getId());
            }

            String jsonParam = URLEncoder.encode(request.getOrderParameters(), StandardCharsets.UTF_8);
            HttpRequest reConfirmOrderRequest = HttpRequest.newBuilder().uri(new URI("https://" + NetworkUtil.getRandomDomain() + "/vbuy/ReConfirmOrder/1.0")).header("Content-Type", "application/x-www-form-urlencoded;charset=UTF-8").header("Cookie", request.getCookies()).header("Referer", "https://android.weidian.com/").header("User-Agent", "Android/9 WDAPP(WDBuyer/7.6.2) Thor/2.3.25").POST(BodyPublishers.ofString("param=" + jsonParam)).build();
            HttpRequest.newBuilder().uri(new URI("https://" + NetworkUtil.getRandomDomain() + "/vbuy/CreateOrder/1.0")).header("Content-Type", "application/x-www-form-urlencoded;charset=UTF-8").header("Cookie", request.getCookies()).header("Referer", "https://android.weidian.com/").header("User-Agent", "Android/9 WDAPP(WDBuyer/7.6.2) Thor/2.3.25").POST(BodyPublishers.ofString("param=" + jsonParam)).build();
            HttpRequest createOrderRequest = HttpRequest.newBuilder().uri(new URI("https://thor.weidian.com/vbuy/CreateOrder/1.0")).header("Content-Type", "application/x-www-form-urlencoded;charset=UTF-8").header("Cookie", request.getCookies()).header("Referer", "https://android.weidian.com/").header("User-Agent", "Android/9 WDAPP(WDBuyer/7.6.2) Thor/2.3.25").POST(BodyPublishers.ofString("param=" + jsonParam)).build();
            LocalDateTime startTime = request.getStartTime();
            LocalDateTime endTime = request.getEndTime();
            long needDelay = (long)request.getDelay();
            boolean steadyOrder = extensionNode.get("steadyOrder").asBoolean();
            boolean autoPick = extensionNode.get("autoPick").asBoolean();
            synchronized(this) {
                if (Duration.between(this.updateTime, LocalDateTime.now()).toMillis() > 5000L) {
                    this.adjustedFactor = CommonUtil.getAdjustedFactor(request);
                    this.updateTime = LocalDateTime.now();
                    logger.info("ID：{} 重新计算平均网络延迟：{}ms", request.getId(), this.adjustedFactor);
                } else {
                    logger.info("ID：{} 使用上次计算的平均网络延迟：{}ms", request.getId(), this.adjustedFactor);
                }
            }

            long delay = needDelay - this.adjustedFactor - this.precessingTime;
            if (delay < 500L) {
                long waitTime = Duration.between(LocalDateTime.now(), startTime).toMillis() - this.adjustedFactor - 3000L;
                TimeUnit.MILLISECONDS.sleep(waitTime);
                if (Duration.between(this.prestartTime, LocalDateTime.now()).toMillis() > 50000L) {
                    NetworkUtil.sendAsyncExhibitSpaceJsonRequest(request);
                }
            }

            logger.info("ID：{} 等待：{} ms 延迟: {}ms 预计{}开始抢购", new Object[]{request.getId(), Duration.between(LocalDateTime.now(), startTime).toMillis(), delay, startTime});
            long timeRemaining = Math.max(0L, Duration.between(LocalDateTime.now(), startTime).toMillis() + delay);
            scheduler.schedule(() -> {
                try {
                    JsonNode createOrderResult = createOrder(createOrderRequest, request);
                    List<Map<String, Object>> orderResponses = new ArrayList();
                    this.recordOrderResponse(orderResponses, createOrderResult, 1);
                    LocalDateTime presentTime = LocalDateTime.now();
                    int count = 1;

                    long initialDelay = 150L;
                    double backoffFactor = 2.0;

                    long maxDelay = 800L;

                    int maxCount = 40;

                    long currentDelay = initialDelay;  // 当前延迟时间

                    if (createOrderResult != null) {
                        while(createOrderResult.get("isContinue") != null && createOrderResult.get("isContinue").asBoolean()) {
                            if (steadyOrder || createOrderResult.get("isUpdate") != null && createOrderResult.get("isUpdate").asBoolean()) {
                                long presentMillis = System.currentTimeMillis();
                                TimeUnit.MILLISECONDS.sleep(300L);
                                JsonNode reConfirmOrderResult = reConfirmOrder(reConfirmOrderRequest);
                                logger.info("ID：{}更新订单参数", request.getId());
                                System.out.println(reConfirmOrderResult);
                                JsonNode quickParam = (JsonNode)RetryUtil.retryWithExponentialBackoff(() -> CommonUtil.generateOrderParameters(request, reConfirmOrderResult, !steadyOrder), (result) -> result != null, 5, 50L, 1000, "ID：" + request.getId() + " 更新订单参数");
                                if (quickParam == null) {
//                                    logger.error("ID：{} 更新订单参数失败，尝试直接使用当前订单参数", request.getId());
                                    logger.error("ID：{} 更新订单参数失败，重新尝试", request.getId());
//                                    quickParam = mapper.readTree(request.getOrderParameters());
                                    continue;

                                } else {
                                    logger.info("ID：{} 更新订单参数成功", request.getId());
                                }

                                String stringParam = URLEncoder.encode(mapper.writeValueAsString(quickParam), StandardCharsets.UTF_8);
                                HttpRequest updatedCreateOrderRequest = HttpRequest.newBuilder().uri(new URI("https://thor.weidian.com/vbuy/CreateOrder/1.0")).header("Content-Type", "application/x-www-form-urlencoded;charset=UTF-8").header("Cookie", request.getCookies()).header("Referer", "https://android.weidian.com").POST(BodyPublishers.ofString("param=" + stringParam)).build();
                                TimeUnit.MILLISECONDS.sleep(Math.max(700L - (System.currentTimeMillis() - presentMillis) - this.adjustedFactor - this.precessingTime, 0L));
                                createOrderResult = createOrder(updatedCreateOrderRequest, request);
                                ++count;
                                this.recordOrderResponse(orderResponses, createOrderResult, count);
                            }
                            else {
                                TimeUnit.MILLISECONDS.sleep(currentDelay);
                                if(count==(maxCount/2)) {
                                    currentDelay = initialDelay;
                                }else{
                                    currentDelay = Math.min((long)(currentDelay * backoffFactor), maxDelay);
                                }
                                createOrderResult = createOrder(createOrderRequest, request);
                                ++count;
                                this.recordOrderResponse(orderResponses, createOrderResult, count);

                            }

                            presentTime = LocalDateTime.now();
                            if (count >= maxCount) {
                                ((ObjectNode)createOrderResult).put("isContinue", false);
                                break;
                            }
                        }
                    }

                    ((ObjectNode)createOrderResult).put("count", count);
                    ObjectNode finalResult = (ObjectNode)createOrderResult;
                    finalResult.set("responses_history", mapper.valueToTree(orderResponses));
                    Results grabResult = this.convertAndInsert(request);
                    grabResult.setResponseMessage(finalResult.toString());
                    grabResult.setEndTime(presentTime);
                    grabResult.setStatus(createOrderResult.get("isSuccess").asInt());
                    if (grabResult.getStatus() == 2 && autoPick) {
                        request.setId((Integer)null);
                        request.setStatus(1);
                        request.setType(3);
                        this.executePick(request);
                        grabResult.setActualEarnings(BigDecimal.valueOf(0L));
                    }

                    this.resultsMapper.insert(grabResult);
                    MailService.sendSuccesedEmail(grabResult);
                } catch (Exception e) {
                    e.printStackTrace();
                    Logger var10000 = logger;
                    Integer var10001 = request.getId();
                    var10000.error("ID：" + var10001 + "抢购请求失败：" + e);
                    Results grabResult = this.convertAndInsert(request);
                    JsonNode jsonMessage = mapper.createObjectNode();
                    ((ObjectNode)jsonMessage).putPOJO("error_message", getFirstThreeLinesOfStackTrace(e));
                    grabResult.setResponseMessage(jsonMessage.toString());
                    grabResult.setEndTime(LocalDateTime.now());
                    grabResult.setStatus(3);
                    this.resultsMapper.insert(grabResult);

                    try {
                        MailService.sendFailedGrabEmail(grabResult);
                    } catch (JsonProcessingException e1) {
                        var10000 = logger;
                        var10001 = request.getId();
                        var10000.error("ID：" + var10001 + " 发送失败邮件通知失败：" + e1.getMessage());
                    }
                }

                this.requestsMapper.deleteByPrimaryKey(request.getId());
                logger.info("ID：{} 抢购结束", request.getId());
            }, timeRemaining, TimeUnit.MILLISECONDS);
        } catch (Throwable $ex) {
//            throw $ex;
        }
    }

    private void recordOrderResponse(List<Map<String, Object>> orderResponses, JsonNode response, int attemptNumber) {
        try {
            Map<String, Object> responseRecord = new HashMap();
            String timestamp = LocalDateTime.now().format(DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss.SSS"));
            responseRecord.put("timestamp", timestamp);
            responseRecord.put("attempt", attemptNumber);
            if (response != null && response.has("status")) {
                responseRecord.put("status", response.get("status"));
            } else {
                responseRecord.put("status", (Object)null);
            }

            orderResponses.add(responseRecord);
        } catch (Exception e) {
            logger.error("记录订单响应信息失败: {}", e.getMessage());
        }

    }

    public void executePick(Requests request) {
        scheduler.schedule(() -> {
            try {
                this.requestsMapper.updateThreadIdById(request.getId(), String.valueOf(Thread.currentThread().getId()));
                JsonNode extensionNode = mapper.readTree(request.getExtension());
                boolean quickMode = extensionNode.has("quickMode") && extensionNode.get("quickMode").asBoolean();
                if (!quickMode) {
                    try {
                        JsonNode addOrderData = NetworkUtil.getAddOrderData(request);
                        if (addOrderData != null) {
                            JsonNode orderParams = (JsonNode)RetryUtil.retryWithExponentialBackoff(() -> CommonUtil.generateOrderParameters(request, addOrderData, true), (resultx) -> resultx != null, 3, 100L, 20, "ID：" + request.getId() + " 生成订单参数");
                            if (orderParams != null) {
                                request.setOrderParameters(orderParams.toString());
                                logger.info("ID：{} 生成订单参数成功", request.getId());
                            } else {
                                logger.error("ID：{} 生成订单参数失败，将使用现有参数继续执行", request.getId());
                            }
                        }
                    } catch (Exception e) {
                        logger.error("ID：{} 生成订单参数过程出现严重错误: {}", request.getId(), e.getMessage());
                    }
                } else {
                    logger.info("ID：{} 使用快速模式，跳过重新生成订单参数", request.getId());
                }

                String paramStr = request.getOrderParameters();
                JsonNode paramNode = mapper.readTree(paramStr);
                JsonNode firstItemNode = null;
                JsonNode shopList = paramNode.get("shop_list");
                if (shopList != null && shopList.isArray() && shopList.size() > 0) {
                    JsonNode firstShop = shopList.get(0);
                    JsonNode itemList = firstShop.get("item_list");
                    if (itemList != null && itemList.isArray() && itemList.size() > 0) {
                        firstItemNode = itemList.get(0);
                    }
                }

                String jsonParam = URLEncoder.encode(mapper.writeValueAsString(paramNode), StandardCharsets.UTF_8);
                HttpRequest createOrderRequest = HttpRequest.newBuilder().uri(new URI("https://" + NetworkUtil.getRandomDomain() + "/vbuy/CreateOrder/1.0")).header("Content-Type", "application/x-www-form-urlencoded;charset=UTF-8").header("Cookie", request.getCookies()).header("Referer", "https://android.weidian.com").POST(BodyPublishers.ofString("param=" + jsonParam)).build();
                JsonNode responseBody = null;
                LocalDateTime endTime = request.getEndTime();
                int count = 0;
                int stock = 0;
                boolean virtualItem = false;
                int frequency = request.getFrequency();
                frequency = Math.max(frequency - 150, 100);
                logger.info("ID：{}检查商品库存", request.getId());

                do {
                    JsonNode result = NetworkUtil.getSingleGoodsInventory(firstItemNode);
                    if (result != null && result.has("status") && result.get("status").has("code")) {
                        int statusCode = result.get("status").get("code").intValue();

                        if (statusCode != 0 && statusCode != 3) {
                            if (statusCode == 12) {
                                logger.info("ID：" + request.getId() + "商品不支持加购物车");
                                virtualItem = true;
                                break;
                            }
                        } else {
                            logger.info("ID：" + request.getId() + "商品有货，开始抢购");
                            responseBody = createOrder(createOrderRequest, request);
                            if (responseBody != null && !responseBody.get("isContinue").asBoolean()) {
                                ((ObjectNode)responseBody).put("count", count);
                                break;
                            }
                        }
                    } else if (result == null) {
                        logger.error("ID：" + request.getId() + "获取商品库存失败");
                    }

                    ++count;
                    TimeUnit.MILLISECONDS.sleep((long)frequency);
                } while(LocalDateTime.now().isBefore(endTime));

                if (virtualItem) {
                    do {
                        JsonNode var27 = NetworkUtil.getItemSkuInfoByItemId(firstItemNode.get("item_id").asText()).get("result");
                        if (var27 != null) {
                            if (firstItemNode.get("item_sku_id").asText().equals("0")) {
                                stock = var27.get("itemStock").asInt();
                            } else {
                                stock = Integer.parseInt(FindUtil.findObjectElementStringValueByOtherValue(var27.get("skuInfos"), "id", firstItemNode.get("item_sku_id").asText(), "stock"));
                            }

                            if (stock > 0) {
                                logger.info("ID：" + request.getId() + "商品有货，开始抢购");
                                responseBody = createOrder(createOrderRequest, request);
                                if (responseBody != null && !responseBody.get("isContinue").asBoolean()) {
                                    ((ObjectNode)responseBody).put("count", count);
                                    break;
                                }
                            }
                        } else {
                            logger.error("ID：" + request.getId() + "获取商品库存失败");
                        }

                        ++count;
                        TimeUnit.MILLISECONDS.sleep((long)frequency);
                    } while(LocalDateTime.now().isBefore(endTime));
                }

                if (responseBody == null) {
                    logger.info("ID：" + request.getId() + " 捡漏超时");
                    Results grabResult = this.convertAndInsert(request);
                    grabResult.setResponseMessage("{\"status\":{\"code\":400,\"message\":\"抢购失败\",\"description\":\"运行超时\"},\"result\":null}");
                    grabResult.setEndTime(LocalDateTime.now());
                    grabResult.setStatus(2);
                    this.resultsMapper.insert(grabResult);
                } else {
                    logger.info("ID：" + request.getId() + " 捡漏结束");
                    Results grabResult = this.convertAndInsert(request);
                    grabResult.setResponseMessage(responseBody.toString());
                    grabResult.setEndTime(LocalDateTime.now());
                    grabResult.setStatus(responseBody.get("isSuccess").asInt());
                    this.resultsMapper.insert(grabResult);
                    MailService.sendSuccesedEmail(grabResult);
                }
            } catch (Exception e) {
                e.printStackTrace();
                Logger var10000 = logger;
                Integer var10001 = request.getId();
                var10000.error("ID：" + var10001 + " 抢购请求失败：" + e);
                Results grabResult = this.convertAndInsert(request);
                JsonNode jsonMessage = mapper.createObjectNode();
                ((ObjectNode)jsonMessage).putPOJO("error_message", getFirstThreeLinesOfStackTrace(e));
                grabResult.setResponseMessage(jsonMessage.toString());
                grabResult.setEndTime(LocalDateTime.now());
                grabResult.setStatus(3);
                this.resultsMapper.insert(grabResult);

                try {
                    MailService.sendFailedGrabEmail(grabResult);
                } catch (JsonProcessingException e1) {
                    var10000 = logger;
                    var10001 = request.getId();
                    var10000.error("ID：" + var10001 + " 发送失败邮件通知失败：" + e1.getMessage());
                }
            }

            this.requestsMapper.deleteByPrimaryKey(request.getId());
        }, 0L, TimeUnit.MILLISECONDS);
    }

    public void checkItems(Requests request) {
        scheduler.schedule(() -> {
            this.requestsMapper.updateThreadIdById(request.getId(), String.valueOf(Thread.currentThread().getId()));

            try {
                LocalDateTime startTime = request.getStartTime();
                LocalDateTime endTime = request.getEndTime();
                String titleContent = request.getKeyword();
                if (titleContent == null || titleContent.isEmpty()) {
                    titleContent = "";
                }

                String[] titleContents = titleContent.split("\\|", 2);
                String itemTitle = titleContents.length > 0 ? titleContents[0] : "";
                String skuTitle = titleContents.length > 1 ? titleContents[1] : "";
                URL url = new URL(request.getLink());
                String query = URLDecoder.decode(url.getQuery(), StandardCharsets.UTF_8.toString());
                String userId = FindUtil.extractUserId(query);
                logger.info("ID：{}UserID: {}商品名称：{} 商品规格：{}", new Object[]{request.getId(), userId, itemTitle, skuTitle});
                int frequency = request.getFrequency();
                frequency = Math.max(frequency - 100, 100);
                String itemId = "";

                while(itemId == "") {
                    JsonNode response = NetworkUtil.getShopItemList(userId);
                    logger.info("ID：{}获取商品列表：", request.getId());
                    itemId = CommonUtil.findMatchingItem(response, startTime, itemTitle);
                    if (itemId != "") {
                        logger.info("ID：{}找到商品：{}", request.getId(), itemId);
                        JsonNode responseJson = NetworkUtil.getItemSkuInfo(itemId);
                        if (responseJson != null) {
                            String skuId = CommonUtil.findMatchingSku(responseJson, skuTitle);
                            if (skuId != "") {
                                String link = String.format("https://weidian.com/buy/add-order/index.php?items=%s_%d_0_%s&source_id=%s", itemId, request.getQuantity(), skuId, "6df14f35dae7e6e49e1a944cd2ad4adf");
                                logger.info("ID：{}找到规格：{} 生成链接：{}", new Object[]{request.getId(), skuId, link});
                                request.setLink(link);
                                JsonNode processedTemplate = CommonUtil.processOrderTemplateWithPrediction(request);
                                if (processedTemplate != null) {
                                    logger.info("ID：{} 预测规则处理成功", request.getId());
                                }

                                this.executeOrder(request);

                                try {
                                    MailService.sendFoundItemEmail(request, link);
                                } catch (Exception e) {
                                    logger.error("ID：{}发送找到商品邮件通知失败：{}", request.getId(), e.getMessage());
                                }
                            }
                        }
                        break;
                    }

                    if (LocalDateTime.now().isAfter(endTime)) {
                        logger.info("ID：{}超出抢购时间，退出循环", request.getId());
                        Results grabResult = this.convertAndInsert(request);
                        grabResult.setResponseMessage("{\"status\":{\"code\":400,\"message\":\"抢购失败\",\"description\":\"运行超时\"},\"result\":null}");
                        grabResult.setEndTime(LocalDateTime.now());
                        grabResult.setStatus(2);
                        this.resultsMapper.insert(grabResult);
                        break;
                    }

                    Thread.sleep((long)frequency);
                }
            } catch (Exception e) {
                e.printStackTrace();
                Logger var10000 = logger;
                Integer var10001 = request.getId();
                var10000.error("ID：" + var10001 + "抢购请求失败：" + e);
                Results grabResult = this.convertAndInsert(request);
                JsonNode jsonMessage = mapper.createObjectNode();
                ((ObjectNode)jsonMessage).putPOJO("error_message", getFirstThreeLinesOfStackTrace(e));
                grabResult.setResponseMessage(jsonMessage.toString());
                grabResult.setEndTime(LocalDateTime.now());
                grabResult.setStatus(3);
                this.resultsMapper.insert(grabResult);

                try {
                    MailService.sendFailedGrabEmail(grabResult);
                } catch (JsonProcessingException e1) {
                    var10000 = logger;
                    var10001 = request.getId();
                    var10000.error("ID：" + var10001 + " 发送失败邮件通知失败：" + e1.getMessage());
                }
            }

        }, 0L, TimeUnit.MILLISECONDS);
    }

    @Async
    public void executeOrder(Requests request) throws URISyntaxException, JsonProcessingException {
        JsonNode extensionNode = mapper.readTree(request.getExtension());
        boolean quickMode = extensionNode.has("quickMode") && extensionNode.get("quickMode").asBoolean();
        if (!quickMode) {
            JsonNode addOrderData = NetworkUtil.getAddOrderData(request);
            if (addOrderData != null) {
                request.setOrderParameters(CommonUtil.generateOrderParameters(request, addOrderData, true).toString());
            }
        } else {
            logger.info("ID：{} 使用快速模式，跳过重新生成订单参数", request.getId());
        }

        String domain = NetworkUtil.getRandomDomain();
        String jsonParam = URLEncoder.encode(request.getOrderParameters(), StandardCharsets.UTF_8);
        HttpRequest reConfirmOrderRequest = HttpRequest.newBuilder().uri(new URI("https://" + domain + "/vbuy/ReConfirmOrder/1.0")).header("Content-Type", "application/x-www-form-urlencoded;charset=UTF-8").header("Cookie", request.getCookies()).header("Referer", "https://android.weidian.com/").header("User-Agent", "Android/9 WDAPP(WDBuyer/7.6.2) Thor/2.3.25").POST(BodyPublishers.ofString("param=" + jsonParam)).build();
        HttpRequest CreateOrderRequest = HttpRequest.newBuilder().uri(new URI("https://" + domain + "/vbuy/CreateOrder/1.0")).header("Content-Type", "application/x-www-form-urlencoded;charset=UTF-8").header("Cookie", request.getCookies()).header("Referer", "https://android.weidian.com/").header("User-Agent", "Android/9 WDAPP(WDBuyer/7.6.2) Thor/2.3.25").POST(BodyPublishers.ofString("param=" + jsonParam)).build();
        HttpRequest mainCreateOrderRequest = HttpRequest.newBuilder().uri(new URI("https://thor.weidian.com/vbuy/CreateOrder/1.0")).header("Content-Type", "application/x-www-form-urlencoded;charset=UTF-8").header("Cookie", request.getCookies()).header("Referer", "https://android.weidian.com/").header("User-Agent", "Android/9 WDAPP(WDBuyer/7.6.2) Thor/2.3.25").POST(BodyPublishers.ofString("param=" + jsonParam)).build();
        HttpRequest preCreateOrderRequest = HttpRequest.newBuilder().uri(new URI("https://thor.weidian.com/vbuy/CreateOrder/1.0")).header("Content-Type", "application/x-www-form-urlencoded;charset=UTF-8").header("Cookie", request.getCookies()).header("Referer", "https://android.weidian.com/").header("User-Agent", "Android/9 WDAPP(WDBuyer/7.6.2) Thor/2.3.25").POST(BodyPublishers.ofString("param=")).build();
        long needDelay = (long)request.getDelay();
        LocalDateTime startTime = request.getStartTime();
        boolean steadyOrder = extensionNode.get("steadyOrder").asBoolean();
        boolean autoPick = extensionNode.get("autoPick").asBoolean();
        long delay = needDelay - this.adjustedFactor - this.precessingTime;
        long timeRemaining = Math.max(0L, Duration.between(LocalDateTime.now(), startTime).toMillis() + delay);
        logger.info("ID：{} 等待：{} ms 需要延迟 {}ms 线程启动耗时 {}ms 加上延迟等待：{}ms开始抢购", new Object[]{request.getId(), Duration.between(LocalDateTime.now(), startTime).toMillis(), needDelay, this.schedulingTime, timeRemaining});
        scheduler.schedule(() -> {
            try {
                JsonNode createOrderResult = createOrder(mainCreateOrderRequest, request);
                LocalDateTime presentTime = LocalDateTime.now();
                int count = 1;
                if (createOrderResult != null) {
                    while(createOrderResult.get("isContinue") != null && createOrderResult.get("isContinue").asBoolean()) {
                        if (steadyOrder || createOrderResult.get("isUpdate") != null && createOrderResult.get("isUpdate").asBoolean()) {
                            long presentMillis = System.currentTimeMillis();
                            Logger var22 = logger;
                            Integer var24 = request.getId();
                            var22.info("ID：" + var24 + " 更新订单参数");
                            JsonNode reConfirmOrderResult = reConfirmOrder(reConfirmOrderRequest);
                            JsonNode quickParam = CommonUtil.generateOrderParameters(request, reConfirmOrderResult, !steadyOrder);
                            String stringParam = URLEncoder.encode(mapper.writeValueAsString(quickParam), StandardCharsets.UTF_8);
                            HttpRequest updatedCreateOrderRequest = HttpRequest.newBuilder().uri(new URI("https://" + NetworkUtil.getRandomDomain() + "/vbuy/CreateOrder/1.0")).header("Content-Type", "application/x-www-form-urlencoded;charset=UTF-8").header("Cookie", request.getCookies()).header("Referer", "https://android.weidian.com").POST(BodyPublishers.ofString("param=" + stringParam)).build();
                            TimeUnit.MILLISECONDS.sleep(Math.max(1000L - (System.currentTimeMillis() - presentMillis), 0L));
                            createOrderResult = createOrder(updatedCreateOrderRequest, request);
                        } else {
                            TimeUnit.MILLISECONDS.sleep(985L);
                            createOrderResult = createOrder(CreateOrderRequest, request);
                        }

                        presentTime = LocalDateTime.now();
                        ++count;
                        if (count >= 10) {
                            ((ObjectNode)createOrderResult).put("isContinue", false);
                            break;
                        }
                    }
                }

                ((ObjectNode)createOrderResult).put("count", count);
                Results grabResult = this.convertAndInsert(request);
                grabResult.setResponseMessage(createOrderResult.toString());
                grabResult.setEndTime(presentTime);
                grabResult.setStatus(createOrderResult.get("isSuccess").asInt());
                if (grabResult.getStatus() == 2 && autoPick) {
                    request.setId((Integer)null);
                    request.setStatus(1);
                    request.setType(3);
                    this.executePick(request);
                    grabResult.setActualEarnings(BigDecimal.valueOf(0L));
                }

                this.resultsMapper.insert(grabResult);
                MailService.sendSuccesedEmail(grabResult);
            } catch (Exception e) {
                e.printStackTrace();
                Logger var10000 = logger;
                Integer var10001 = request.getId();
                var10000.error("ID：" + var10001 + "抢购请求失败：" + e);
                Results grabResult = this.convertAndInsert(request);
                JsonNode jsonMessage = mapper.createObjectNode();
                ((ObjectNode)jsonMessage).putPOJO("error_message", getFirstThreeLinesOfStackTrace(e));
                grabResult.setResponseMessage(jsonMessage.toString());
                grabResult.setEndTime(LocalDateTime.now());
                grabResult.setStatus(3);
                this.resultsMapper.insert(grabResult);

                try {
                    MailService.sendFailedGrabEmail(grabResult);
                } catch (JsonProcessingException e1) {
                    var10000 = logger;
                    var10001 = request.getId();
                    var10000.error("ID：" + var10001 + " 发送失败邮件通知失败：" + e1.getMessage());
                }
            }

            this.requestsMapper.deleteByPrimaryKey(request.getId());
        }, timeRemaining, TimeUnit.MILLISECONDS);
    }

    public boolean handleRequest(Requests request) {
        try {
            this.requestsMapper.insert(request);
            System.out.println(request);
            logger.info("ID：{}请求已插入数据库", request.getId());
            return true;
        } catch (Exception e) {
            e.printStackTrace();
            logger.error(e.toString());
            return false;
        }
    }

    public void checkLinkValid(Requests request) throws JsonProcessingException {
        JsonNode result = NetworkUtil.sendReConfirmOrderRequest(request);
        if (result != null) {
            long networkDelay = result.get("networkDelay").asLong();
            long takeTime = result.get("takeTime").asLong();
            logger.info("ID：{} 网络延迟：{}ms 耗时：{}ms", new Object[]{request.getId(), networkDelay, takeTime});

            for(JsonNode item : result.get("invalid_item_list")) {
                String reason = item.get("reason").asText();
                if (reason.contains("删除") || reason.contains("变更") || reason.contains("限购")) {
                    logger.info("ID：{} 商品已删除、限购或变更", request.getId());
                    MailService.sendFailEmailInfo(request, item);
                    break;
                }
            }

        }
    }

    private static String getFirstThreeLinesOfStackTrace(Exception e) {
        StringWriter sw = new StringWriter();
        PrintWriter pw = new PrintWriter(sw);
        e.printStackTrace(pw);
        String stackTrace = sw.toString();
        String[] lines = stackTrace.split(System.lineSeparator());
        StringBuilder firstThreeLines = new StringBuilder();

        for(int i = 0; i < Math.min(3, lines.length); ++i) {
            firstThreeLines.append(lines[i]).append(System.lineSeparator());
        }

        return firstThreeLines.toString();
    }

    public Results convertAndInsert(Requests request) {
        Results result = new Results();
        Field[] requestFields = request.getClass().getDeclaredFields();
        Field[] resultFields = result.getClass().getDeclaredFields();
        Set<String> resultFieldNames = (Set)Arrays.stream(resultFields).map(Field::getName).collect(Collectors.toSet());

        for(Field requestField : requestFields) {
            if (!requestField.getName().equals("id") && resultFieldNames.contains(requestField.getName())) {
                try {
                    requestField.setAccessible(true);
                    Field resultField = result.getClass().getDeclaredField(requestField.getName());
                    resultField.setAccessible(true);
                    if (requestField.getType() == resultField.getType() && !Modifier.isFinal(resultField.getModifiers()) && !Modifier.isStatic(resultField.getModifiers())) {
                        resultField.set(result, requestField.get(request));
                    }
                } catch (IllegalAccessException | NoSuchFieldException e) {
                    ((ReflectiveOperationException)e).printStackTrace();
                }
            }
        }

        return result;
    }
}
