//
// Source code recreated from a .class file by IntelliJ IDEA
// (powered by FernFlower decompiler)
//

package indi.wookat.quickgrab.util;

import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.node.ArrayNode;
import com.fasterxml.jackson.databind.node.ObjectNode;
import indi.wookat.quickgrab.entity.Requests;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.net.URLDecoder;
import java.nio.charset.StandardCharsets;
import java.text.DecimalFormat;
import java.time.LocalDateTime;
import java.time.ZoneOffset;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.stream.Stream;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Component;
import org.springframework.web.bind.annotation.RequestBody;

@Component
public class CommonUtil {
    private static final ObjectMapper objectMapper = new ObjectMapper();
    private static final Logger logger = LoggerFactory.getLogger(CommonUtil.class);

    public static JsonNode generateOrderParameters(@RequestBody Requests request, JsonNode dataObj, boolean includeInvalid) {
        if (dataObj == null) {
            return null;
        } else {
            DecimalFormat df = new DecimalFormat("0.00");
            double totalPayPrice = (double)0.0F;
            ObjectNode paramNode = objectMapper.createObjectNode();
            String[] lists = includeInvalid ? new String[]{"invalid_shop_list", "shop_list"} : new String[]{"shop_list"};
            JsonNode itemDataObj = null;
            JsonNode deliveryInfo = null;
            int deliveryInfoType = 1;

            for(String list : lists) {
                JsonNode shopList = dataObj.get(list);
                if (shopList != null && shopList.isArray() && !shopList.isEmpty()) {
                    ArrayNode shopListArray = paramNode.putArray("shop_list");

                    for(JsonNode shop : shopList) {
                        String[] itemLists = includeInvalid ? new String[]{"invalid_item_list", "item_list"} : new String[]{"item_list"};

                        for(String itemListName : itemLists) {
                            JsonNode itemList = shop.get(itemListName);
                            if (itemList != null && itemList.isArray() && !itemList.isEmpty()) {
                                ObjectNode shopNode = objectMapper.createObjectNode();
                                double shopOriPrice = (double)0.0F;
                                double shopPrice = (double)0.0F;
                                double expressFee = (double)0.0F;
                                ArrayNode itemListArray = shopNode.putArray("item_list");

                                for(JsonNode item : itemList) {
                                    ObjectNode itemNode = objectMapper.createObjectNode();
                                    itemNode.put("item_id", item.get("item_id").asText());
                                    itemNode.put("quantity", item.get("quantity").asInt());
                                    itemNode.put("item_sku_id", item.get("item_sku_id").asText());
                                    itemNode.put("price", item.get("price").asDouble());
                                    double oriPrice = item.has("ori_price") ? item.get("ori_price").asDouble() : item.get("price").asDouble();
                                    itemNode.put("ori_price", oriPrice);
                                    if (dataObj.has("confirmOrderParam")) {
                                        for(JsonNode node : dataObj.get("confirmOrderParam").get("item_list")) {
                                            if (node.has("item_id") && node.get("item_id").asText().equals(item.get("item_id").asText()) && node.has("calendar_date")) {
                                                itemNode.put("calendar_date", node.get("calendar_date").asText());
                                            }
                                        }
                                    }

                                    if (item.has("item_convey_info") && item.get("item_convey_info").has("valid_date_info")) {
                                        logger.info("演出票，开始解析有效期:{}", item.get("item_convey_info"));
                                        itemNode.set("item_convey_info", item.get("item_convey_info"));
                                    } else {
                                        try {
                                            JsonNode extensionNode = (JsonNode)(request.getExtension() != null ? objectMapper.readTree(request.getExtension()) : objectMapper.createObjectNode());
                                            if (extensionNode.has("hasExpireDate") && extensionNode.get("hasExpireDate").asBoolean()) {
                                                logger.info("有效期品类，开始解析有效期");
                                                JsonNode itemInfo = NetworkUtil.getItemInfo(item.get("item_id").asText(), request.getCookies());
                                                if (itemInfo != null) {
                                                    try {
                                                        if (itemInfo.has("itemInfo") && itemInfo.get("itemInfo").has("ticketItemInfo") && itemInfo.get("itemInfo").get("ticketItemInfo").has("expireDate")) {
                                                            String expireDate = itemInfo.get("itemInfo").get("ticketItemInfo").get("expireDate").asText();
                                                            JsonNode item_convey_info = parseExpireDate(expireDate);
                                                            itemNode.set("item_convey_info", item_convey_info);
                                                            System.out.println(item_convey_info.toPrettyString());
                                                        }
                                                    } catch (Exception e) {
                                                        e.printStackTrace();
                                                        logger.error("解析有效期失败: {}", e.getMessage());
                                                    }
                                                } else {
                                                    logger.error("获取商品信息失败: {}", item.get("item_id").asText());
                                                }
                                            }
                                        } catch (JsonProcessingException e) {
                                            throw new RuntimeException(e);
                                        }
                                    }

                                    itemListArray.add(itemNode);
                                    shopOriPrice += oriPrice * (double)item.get("quantity").asInt();
                                    shopPrice += item.get("price").asDouble() * (double)item.get("quantity").asInt();
                                }

                                try {
                                    itemDataObj = NetworkUtil.getItemData(itemList.get(0).get("item_id").asText());
                                } catch (Exception e) {
                                    throw new RuntimeException(e);
                                }

                                if (itemDataObj != null) {
                                    deliveryInfo = itemDataObj.get("delivery_info");
                                    if (deliveryInfo != null && deliveryInfo.has("postageInfosNew")) {
                                        JsonNode postageInfosNew = deliveryInfo.get("postageInfosNew");
                                        boolean hasExpress = false;
                                        boolean hasSelfPickup = false;

                                        for(JsonNode info : postageInfosNew) {
                                            if (info.get("deliveryDes").asText().contains("快递")) {
                                                hasExpress = true;
                                            }

                                            if (info.get("deliveryDes").asText().contains("自提")) {
                                                hasSelfPickup = true;
                                            }
                                        }

                                        if (hasExpress && hasSelfPickup) {
                                            deliveryInfoType = 2;
                                        } else if (hasSelfPickup) {
                                            deliveryInfoType = 3;
                                        }
                                    }
                                }

                                if (shop.has("express_list") && shop.get("express_list").isArray()) {
                                    expressFee = shop.get("express_list").get(0).get("express_fee").asDouble();
                                } else {
                                    JsonNode extensionNode = null;

                                    try {
                                        extensionNode = objectMapper.readTree(request.getExtension());
                                        if (extensionNode.has("manualShipping") && extensionNode.get("manualShipping").asBoolean() && extensionNode.has("shippingFee")) {
                                            expressFee = extensionNode.get("shippingFee").asDouble();
                                            logger.info("ID：{} 使用手动设置的邮费：{}元", request.getId(), expressFee);
                                        } else {
                                            logger.info("开始获取快递费用");
                                            if (deliveryInfo != null && deliveryInfo.has("expressPostageDesc")) {
                                                String expressPostageDesc = deliveryInfo.get("expressPostageDesc").asText();
                                                boolean isFreeShippingThresholdMet = false;
                                                Pattern pattern = Pattern.compile("满\\d+元包邮");
                                                if (expressPostageDesc != null) {
                                                    Matcher matcher = pattern.matcher(expressPostageDesc);
                                                    if (matcher != null && matcher.find()) {
                                                        String freeShippingThresholdStr = matcher.group(0).replace("满", "").replace("元包邮", "");
                                                        double freeShippingThreshold = Double.parseDouble(freeShippingThresholdStr);
                                                        if (shopPrice >= freeShippingThreshold) {
                                                            isFreeShippingThresholdMet = true;
                                                            expressFee = (double)0.0F;
                                                        }
                                                    }

                                                    if (!isFreeShippingThresholdMet) {
                                                        Pattern feePattern = Pattern.compile("(\\d+(?:\\.\\d+)?)元起?");
                                                        Matcher feeMatcher = feePattern.matcher(expressPostageDesc);
                                                        if (feeMatcher.find()) {
                                                            String feePart = feeMatcher.group(1);
                                                            expressFee = Double.parseDouble(feePart);
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    } catch (Exception e) {
                                        logger.error("ID：{} 处理邮费设置时出错: {}", request.getId(), e);
                                        logger.info("开始获取快递费用");
                                        if (deliveryInfo != null && deliveryInfo.has("expressPostageDesc")) {
                                            String expressPostageDesc = deliveryInfo.get("expressPostageDesc").asText();
                                            boolean isFreeShippingThresholdMet = false;
                                            Pattern pattern = Pattern.compile("满\\d+元包邮");
                                            if (expressPostageDesc != null) {
                                                Matcher matcher = pattern.matcher(expressPostageDesc);
                                                if (matcher != null && matcher.find()) {
                                                    String freeShippingThresholdStr = matcher.group(0).replace("满", "").replace("元包邮", "");
                                                    double freeShippingThreshold = Double.parseDouble(freeShippingThresholdStr);
                                                    if (shopPrice >= freeShippingThreshold) {
                                                        isFreeShippingThresholdMet = true;
                                                        expressFee = (double)0.0F;
                                                    }
                                                }

                                                if (!isFreeShippingThresholdMet) {
                                                    Pattern feePattern = Pattern.compile("(\\d+(?:\\.\\d+)?)元起?");
                                                    Matcher feeMatcher = feePattern.matcher(expressPostageDesc);
                                                    if (feeMatcher.find()) {
                                                        String feePart = feeMatcher.group(1);
                                                        expressFee = Double.parseDouble(feePart);
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                                shopPrice += expressFee;
                                shopNode.put("shop_id", shop.get("shop").get("shop_id").asText());
                                shopNode.set("item_list", itemListArray);
                                shopNode.put("order_type", 3);
                                shopNode.put("ori_price", df.format(shopOriPrice));
                                shopNode.put("price", df.format(shopPrice));
                                shopNode.put("express_fee", df.format(expressFee));
                                String Message = request.getMessage();
                                if (Message != null && !Message.isEmpty()) {
                                    shopNode.put("note", Message);
                                }

                                shopListArray.add(shopNode);
                                totalPayPrice += shopPrice;
                            }
                        }
                    }
                }
            }

            String orderTemplate = request.getOrderTemplate();
            if (orderTemplate != null && !orderTemplate.isEmpty() && !orderTemplate.equals("null")) {
                try {
                    JsonNode orderTemplateJsonNode = objectMapper.readTree(orderTemplate);
                    paramNode.set("custom_info", orderTemplateJsonNode);
                } catch (JsonProcessingException e) {
                    logger.error("下单模板解析失败: " + e.getMessage());
                }
            }

            ObjectNode buyerInfo = objectMapper.createObjectNode();
            String idCardFlag = FindUtil.findValueByKey(dataObj, "id_card_flag");
            if (idCardFlag != null && idCardFlag.equals("1")) {
                String buyerIdNoCode = request.getIdNumber();
                if (buyerIdNoCode != null && !buyerIdNoCode.isEmpty()) {
                    buyerInfo.put("buyer_id_no_code", buyerIdNoCode);
                }
            }

            String isNoShipAddr = FindUtil.findValueByKey(dataObj, "is_no_ship_addr");
            if (isNoShipAddr != null && isNoShipAddr.equals("1")) {
                paramNode.put("is_no_ship_addr", 1);
            } else {
                paramNode.put("is_no_ship_addr", 0);
            }

            String deliverType = dataObj.has("only_self_delivery") ? dataObj.get("only_self_delivery").asText() : null;
            JsonNode expressTypes = dataObj.has("express_types") ? dataObj.get("express_types") : null;
            if (deliverType != null && deliverType.equals("1")) {
                paramNode.put("deliver_type", 1);
                if (dataObj.has("buyer_address") && dataObj.get("buyer_address").has("self_delivery_address") && dataObj.get("buyer_address").get("self_delivery_address").get(0).has("address_id")) {
                    int selfAddressId = dataObj.get("buyer_address").get("self_delivery_address").get(0).get("address_id").asInt();
                    String buyer_name = dataObj.get("buyer_address").get("buyer_name").asText();
                    String buyer_telephone = dataObj.get("buyer_address").get("phone").asText();
                    buyerInfo.put("self_address_id", selfAddressId);
                    buyerInfo.put("buyer_name", buyer_name);
                    buyerInfo.put("buyer_telephone", buyer_telephone);
                }
            } else if (deliverType != null && deliverType.equals("0") && expressTypes != null && expressTypes.has("type_list") && expressTypes.get("type_list").isArray()) {
                boolean hasExpress = false;

                for(JsonNode type : expressTypes.get("type_list")) {
                    if (type.get("desc").asText().equals("快递")) {
                        hasExpress = true;
                    }
                }

                if (hasExpress) {
                    paramNode.put("deliver_type", 0);
                }
            } else if (deliveryInfoType == 3) {
                paramNode.put("deliver_type", 1);
            } else {
                paramNode.put("deliver_type", 0);
            }

            if (dataObj.has("agreement_info_list") && dataObj.get("agreement_info_list").isArray() && !dataObj.get("agreement_info_list").isEmpty()) {
                for(JsonNode agreementInfo : dataObj.get("agreement_info_list")) {
                    if (agreementInfo.has("agreement_type")) {
                        buyerInfo.putArray("agreement_type_list").add(agreementInfo.get("agreement_type"));
                    }
                }
            }

            paramNode.put("total_pay_price", df.format(totalPayPrice));
            paramNode.put("channel", "maijiaban");
            paramNode.put("source_id", dataObj.get("source_id").asText());
            paramNode.set("buyer", buyerInfo);
            logger.info("ID:{} 生成的下单参数: {}", request.getId(), paramNode);
            return paramNode;
        }
    }

    public static JsonNode parseExpireDate(String expireDate) {
        ObjectMapper mapper = new ObjectMapper();
        ObjectNode validDateInfo = mapper.createObjectNode();
        String[] dates = expireDate.replace(" 有效", "").split(" 至 ");
        if (dates.length == 2) {
            validDateInfo.put("validStartDate", dates[0].trim());
            validDateInfo.put("validEndDate", dates[1].trim());
            validDateInfo.put("validType", 4);
        } else {
            validDateInfo.put("validType", 1);
        }

        ObjectNode item_convey_info = mapper.createObjectNode();
        item_convey_info.set("valid_date_info", validDateInfo);
        return item_convey_info;
    }

    public static JsonNode convertLinkToJson(String url) {
        if (url != null && url.contains("items=") && url.contains("source_id=")) {
            ObjectMapper mapper = new ObjectMapper();
            ObjectNode root = mapper.createObjectNode();
            root.putObject("buyer");
            root.put("channel", "maijiaban");
            ArrayNode itemList = root.putArray("item_list");

            try {
                String[] params = url.split("\\?")[1].split("&");
                String itemsParam = "";
                String sourceId = "";

                for(String param : params) {
                    if (param.startsWith("items=")) {
                        itemsParam = URLDecoder.decode(param.split("=")[1], StandardCharsets.UTF_8.toString());
                    } else if (param.startsWith("source_id=")) {
                        sourceId = URLDecoder.decode(param.split("=")[1], StandardCharsets.UTF_8.toString());
                    }
                }

                String[] items = itemsParam.split(",");

                for(String item : items) {
                    String[] itemDetails = item.split("_");
                    if (itemDetails.length >= 4) {
                        ObjectNode itemNode = mapper.createObjectNode();
                        itemNode.put("item_id", itemDetails[0]);
                        itemNode.put("quantity", Integer.parseInt(itemDetails[1]));
                        itemNode.put("price_type", itemDetails[2]);
                        String skuId = itemDetails[3];
                        if (skuId != null && !skuId.isEmpty() && !skuId.equals("__")) {
                            itemNode.put("item_sku_id", skuId);
                        } else {
                            itemNode.put("item_sku_id", "0");
                        }

                        itemNode.put("item_type", "0");
                        itemNode.put("use_installment", 1);
                        itemList.add(itemNode);
                    }
                }

                root.put("source_id", sourceId);
            } catch (Exception e) {
                e.printStackTrace();
            }

            return root;
        } else {
            return null;
        }
    }

    public static JsonNode processCustomInfo(JsonNode infoList) {
        ObjectMapper objectMapper = new ObjectMapper();
        Iterator<JsonNode> iterator = infoList.elements();

        while(iterator.hasNext()) {
            JsonNode infoNode = (JsonNode)iterator.next();
            String format = infoNode.get("format").asText();
            ObjectNode info = (ObjectNode)infoNode;
            switch (format) {
                case "choice":
                    ArrayNode choiceList = (ArrayNode)infoNode.get("choice_list");
                    if (choiceList != null && choiceList.size() > 0) {
                        String firstChoice = choiceList.get(0).get("name").asText();
                        info.put("value", firstChoice);
                        if (infoNode.get("choice_type").asInt() == 2) {
                            ArrayNode choiceValueList = objectMapper.createArrayNode();
                            choiceValueList.add(firstChoice);
                            info.set("choice_value_list", choiceValueList);
                        }
                    }
                    break;
                case "text":
                case "num":
                case "email":
                case "mobile":
                case "idcard":
                case "time":
                case "date":
                case "pic":
                default:
                    info.put("value", "");
            }
        }

        return infoList;
    }

    public static JsonNode processOrders(JsonNode responseBody, Requests request) {
        JsonNode orderList = responseBody.get("result").get("order_list");
        List<Map<String, Object>> orderDetailsList = new ArrayList();
        if (orderList.isArray()) {
            for(JsonNode orderNode : orderList) {
                String orderId = orderNode.get("order_id").asText();
                JsonNode orderDetailData = NetworkUtil.getNewOrderDetailData(request, orderId);
                if (orderDetailData != null) {
                    Map<String, Object> order = new HashMap();
                    String orderLink = FindUtil.findObjectElementStringValueByOtherValue(orderDetailData, "text", "找人代付", "url");
                    JsonNode orderBasicInfo = orderDetailData.get("detail").get("result").get("orderBasicInfo");
                    String addTime = orderBasicInfo.get("addTime").asText();
                    String title = orderBasicInfo.get("statusDetailDesc").get("title").asText();
                    String desc = orderBasicInfo.get("statusDetailDesc").get("desc").asText();
                    order.put("addTime", addTime);
                    order.put("title", title);
                    order.put("desc", desc);
                    order.put("orderLink", orderLink);
                    orderDetailsList.add(order);
                }
            }
        }

        JsonNode orderLinkListNode = objectMapper.valueToTree(orderDetailsList);
        if (responseBody instanceof ObjectNode) {
            ((ObjectNode)responseBody).set("orderLink_list", orderLinkListNode);
        }

        return responseBody;
    }

    public static String findMatchingItem(JsonNode rootNode, LocalDateTime presetTime, String titleContent) {
        for(JsonNode item : rootNode.path("result").path("itemList")) {
            long addTime = item.path("addTime").asLong();
            if (addTime > presetTime.toInstant(ZoneOffset.of("+8")).toEpochMilli()) {
                String itemName = item.path("itemName").asText();
                if (containsAnyKeyword(itemName, titleContent)) {
                    int stock = item.path("stock").asInt();
                    if (stock > 0) {
                        String itemId = item.path("itemId").asText();
                        System.out.println("item Id: " + itemId);
                        return itemId;
                    }
                }
            }
        }

        return "";
    }

    public static String findMatchingSku(JsonNode jsonResponse, String keywords) {
        JsonNode skuInfos = jsonResponse.path("result").path("skuInfos");
        if (skuInfos.isArray() && !skuInfos.isEmpty()) {
            JsonNode firstSkuInfo = skuInfos.get(0).path("skuInfo");
            String defaultId = firstSkuInfo.path("id").asText();

            for(JsonNode skuInfoNode : skuInfos) {
                JsonNode skuInfo = skuInfoNode.path("skuInfo");
                String title = skuInfo.path("title").asText();
                if (containsAnyKeyword(title, keywords)) {
                    return skuInfo.path("id").asText();
                }
            }

            return defaultId;
        } else {
            return "0";
        }
    }

    private static boolean containsAnyKeyword(String itemName, String titleContent) {
        if (titleContent != null && !titleContent.trim().isEmpty()) {
            Objects.requireNonNull(itemName);
            Stream<String> keywords = Arrays.stream(titleContent.split(";"));
            return keywords.anyMatch(itemName::contains);
        } else {
            return true;
        }
    }


    public static long getAdjustedFactor(Requests request) {
        List<Long> delays = new ArrayList();
        int maxTries = 5;
        int successCount = 0;
        int retryCount = 0;

        while(successCount < maxTries && retryCount < 8) {
            Long delay = calculateFactor(request);
            ++retryCount;
            if (delay != null) {
                delays.add(delay);
                ++successCount;
            }
        }

        if (delays.isEmpty()) {
            return 0L;
        } else {
            long adjustedFactor = calculateAdjustedFactor(delays);
            return adjustedFactor;
        }
    }

    private static Long calculateFactor(Requests request) {
        try {
            JsonNode response = NetworkUtil.sendExhibitSpaceJsonRequest(request);
            if (response != null && response.has("networkDelay")) {
                long factor = response.get("networkDelay").asLong();
                return factor;
            } else {
                return null;
            }
        } catch (Exception e) {
            logger.warn("获取网络延迟时发生异常: " + e.getMessage());
            return null;
        }
    }

    private static long calculateAdjustedFactor(List<Long> delays) {
        List<Long> validDelays = new ArrayList();
        long mean = 0L;

        for(Long delay : delays) {
            if (isValidFactor(delay, delays)) {
                validDelays.add(delay);
            } else {
                System.out.println("Invalid factor: " + delay);
            }
        }

        if (!validDelays.isEmpty()) {
            for(Long validDelay : validDelays) {
                mean += validDelay;
            }

            mean /= (long)validDelays.size();
        }

        return mean;
    }

    private static boolean isValidFactor(Long delay, List<Long> delays) {
        double mean = delays.stream().mapToLong(Long::longValue).average().orElse((double)0.0F);
        double stddev = Math.sqrt(delays.stream().mapToDouble((d) -> Math.pow((double)d - mean, (double)2.0F)).average().orElse((double)0.0F));
        return (double)delay >= mean - (double)1.5F * stddev && (double)delay <= mean + (double)1.5F * stddev;
    }

    public static JsonNode processOrderTemplateWithPrediction(Requests request) {
        try {
            String orderTemplate = request.getOrderTemplate();
            if (orderTemplate != null && !orderTemplate.isEmpty()) {
                JsonNode templateNode;
                try {
                    templateNode = objectMapper.readTree(orderTemplate);
                } catch (Exception e) {
                    logger.error("解析orderTemplate失败: " + e.getMessage());
                    return null;
                }

                boolean isPredictionRule = false;
                if (templateNode.isArray() && templateNode.size() > 0) {
                    JsonNode firstNode = templateNode.get(0);
                    isPredictionRule = firstNode.has("keyword") && firstNode.has("value");
                }

                if (!isPredictionRule) {
                    logger.info("orderTemplate已经是下单模板格式，无需处理");
                    return templateNode;
                } else {
                    JsonNode predictionRules = templateNode;
                    JsonNode dataObj = NetworkUtil.getAddOrderData(request);
                    if (dataObj != null && dataObj.has("custom_info")) {
                        JsonNode customInfo = dataObj.get("custom_info");
                        if (!customInfo.isArray()) {
                            return null;
                        } else {
                            System.out.println("customInfo: " + customInfo);
                            ArrayNode updatedCustomInfo = objectMapper.createArrayNode();

                            for(JsonNode item : customInfo) {
                                ObjectNode updatedItem = (ObjectNode)item.deepCopy();
                                String fieldName = item.get("name").asText();
                                String fieldFormat = item.get("format").asText();

                                for(JsonNode rule : predictionRules) {
                                    String keywords = rule.get("keyword").asText();
                                    String value = rule.get("value").asText();
                                    boolean isMatch = false;
                                    String extractedValue = null;

                                    for(String keyword : keywords.split(";")) {
                                        if (keyword.contains("*")) {
                                            String pattern = keyword.replace("*", "(.*)");
                                            Pattern regex = Pattern.compile(pattern);
                                            Matcher matcher = regex.matcher(fieldName);
                                            if (matcher.find()) {
                                                isMatch = true;
                                                String[] parts = fieldName.split("[：:]+", 2);
                                                if (parts.length > 1) {
                                                    extractedValue = parts[1].trim();
                                                }
                                                break;
                                            }
                                        } else if (fieldName.toLowerCase().contains(keyword.toLowerCase())) {
                                            isMatch = true;
                                            break;
                                        }
                                    }

                                    if (isMatch) {
                                        if ("choice".equals(fieldFormat)) {
                                            JsonNode choiceList = item.get("choice_list");
                                            int choiceType = item.get("choice_type").asInt();
                                            String[] valueKeywords = value.split(";");
                                            List<String> includeKeywords = new ArrayList();
                                            List<String> excludeKeywords = new ArrayList();

                                            for(String keyword : valueKeywords) {
                                                if (keyword.startsWith("!")) {
                                                    excludeKeywords.add(keyword.substring(1).toLowerCase());
                                                } else {
                                                    includeKeywords.add(keyword.toLowerCase());
                                                }
                                            }

                                            List<String> matchedChoices = new ArrayList();

                                            for (JsonNode choice : choiceList) {
                                                String choiceName = choice.get("name")
                                                        .asText()
                                                        .toLowerCase();

                                                // 1. 先看是否在排除名单里
                                                boolean isExcluded = excludeKeywords.stream()
                                                        .anyMatch(choiceName::contains);
                                                if (isExcluded) {
                                                    continue;  // 如果包含排除关键字，跳过
                                                }

                                                // 2. 如果有“包含关键字”要求，就必须命中
                                                boolean shouldAdd;
                                                if (!includeKeywords.isEmpty()) {
                                                    shouldAdd = includeKeywords.stream()
                                                            .anyMatch(choiceName::contains);
                                                } else {
                                                    // 没有特别的包含条件，就直接加
                                                    shouldAdd = true;
                                                }

                                                if (shouldAdd) {
                                                    // 用原始名称（非小写）加入结果列表
                                                    matchedChoices.add(choice.get("name").asText());
                                                }
                                            }

                                            if (!matchedChoices.isEmpty()) {
                                                if (choiceType == 1) {
                                                    String firstChoice = (String)matchedChoices.get(0);
                                                    updatedItem.put("value", firstChoice);
                                                    ObjectNode selfChoice = objectMapper.createObjectNode();
                                                    selfChoice.put(firstChoice, true);
                                                    updatedItem.set("self_choice", selfChoice);
                                                } else if (choiceType == 2) {
                                                    String multiChoiceValue = String.join(",", matchedChoices);
                                                    updatedItem.put("value", multiChoiceValue);
                                                    ObjectNode selfChoice = objectMapper.createObjectNode();
                                                    ArrayNode choiceValueList = objectMapper.createArrayNode();

                                                    for(String choice : matchedChoices) {
                                                        selfChoice.put(choice, true);
                                                        choiceValueList.add(choice);
                                                    }

                                                    updatedItem.set("self_choice", selfChoice);
                                                    updatedItem.set("choice_value_list", choiceValueList);
                                                }

                                                logger.info("字段 {} 匹配到选项: {}", fieldName, matchedChoices);
                                            }
                                        } else if ("num".equals(fieldFormat)) {
                                            if (extractedValue != null && extractedValue.matches("\\d+")) {
                                                updatedItem.put("value", extractedValue);
                                            } else if (value.matches("\\d+")) {
                                                updatedItem.put("value", value);
                                            }
                                        } else if ("pic".equals(fieldFormat)) {
                                            updatedItem.put("value", value);
                                        } else if (extractedValue != null) {
                                            updatedItem.put("value", extractedValue);
                                        } else {
                                            updatedItem.put("value", value);
                                        }

                                        logger.info("字段 {} 匹配规则: {}，提取值: {}", new Object[]{fieldName, keywords, extractedValue != null ? extractedValue : value});
                                        break;
                                    }
                                }

                                updatedCustomInfo.add(updatedItem);
                            }

                            System.out.println("updatedCustomInfo: " + updatedCustomInfo);
                            List<JsonNode> unprocessedFields = new ArrayList();

                            for(JsonNode item : updatedCustomInfo) {
                                String format = item.get("format").asText();
                                String name = item.get("name").asText();
                                if (!item.has("value") || item.get("value").asText().isEmpty()) {
                                    if ("choice".equals(format) && item.get("required").asInt() == 1) {
                                        JsonNode choiceList = item.get("choice_list");
                                        if (choiceList != null && choiceList.isArray() && !choiceList.isEmpty()) {
                                            String firstChoice = choiceList.get(0).get("name").asText();
                                            ((ObjectNode)item).put("value", firstChoice);
                                            ObjectNode selfChoice = objectMapper.createObjectNode();
                                            selfChoice.put(firstChoice, true);
                                            ((ObjectNode)item).set("self_choice", selfChoice);
                                            ArrayNode choiceValueList = objectMapper.createArrayNode();
                                            choiceValueList.add(firstChoice);
                                            ((ObjectNode)item).set("choice_value_list", choiceValueList);
                                            logger.info("为必填选择项 {} 设置默认值: {}", name, firstChoice);
                                        }
                                    } else if (!format.equals("choice")) {
                                        String question = name.replaceAll("[=？?]", "").trim();
                                        if (isArithmeticQuestion(question)) {
                                            try {
                                                double result = calculateArithmetic(question);
                                                String resultStr = result == (double)((long)result) ? String.valueOf((long)result) : String.valueOf(result);
                                                ((ObjectNode)item).put("value", resultStr);
                                                logger.info("计算算术题 {} 得到结果: {}", question, resultStr);
                                            } catch (Exception e) {
                                                logger.error("计算算术题 {} 失败: {}", question, e.getMessage());
                                                unprocessedFields.add(item);
                                            }
                                        } else {
                                            unprocessedFields.add(item);
                                        }
                                    }
                                }
                            }

                            if (!unprocessedFields.isEmpty()) {
                                try {
                                    Map<String, String> aiResponses = getBatchDeepSeekResponse(unprocessedFields, request);

                                    for(JsonNode item : unprocessedFields) {
                                        String var72 = item.get("name").asText();
                                        String fieldId = var72 + "|" + item.get("format").asText();
                                        String aiResponse = (String)aiResponses.get(fieldId);
                                        if (aiResponse != null && !aiResponse.isEmpty()) {
                                            ((ObjectNode)item).put("value", aiResponse);
                                            logger.info("使用AI填写表单项 {}: {}", item.get("name").asText(), aiResponse);
                                        }
                                    }
                                } catch (Exception e) {
                                    logger.error("AI批量填写表单项失败: {}", e.getMessage());
                                }
                            }

                            request.setOrderTemplate(objectMapper.writeValueAsString(updatedCustomInfo));
                            return updatedCustomInfo;
                        }
                    } else {
                        return null;
                    }
                }
            } else {
                return null;
            }
        } catch (Exception e) {
            logger.error("处理预测规则失败: {}", e.getMessage());
            return null;
        }
    }

    private static boolean isArithmeticQuestion(String question) {
        return question.matches(".*[0-9]\\d*[+＋加\\-－减×*乘÷/除➕➖✖️➗✖][0-9]\\d*.*");
    }

    private static double calculateArithmetic(String question) {
        question = question.replaceAll("[＋加➕]", "+").replaceAll("[－减➖]", "-").replaceAll("[×乘✖️✖]", "*").replaceAll("[÷除➗]", "/");
        String[] parts = question.split("[+\\-*/]");
        if (parts.length != 2) {
            throw new IllegalArgumentException("无效的算术表达式");
        } else {
            double num1 = Double.parseDouble(parts[0].trim());
            double num2 = Double.parseDouble(parts[1].trim());
            char operator = 0;

            for(char c : question.toCharArray()) {
                if ("+-*/".indexOf(c) != -1) {
                    operator = c;
                    break;
                }
            }

            switch (operator) {
                case '*':
                    return num1 * num2;
                case '+':
                    return num1 + num2;
                case ',':
                case '.':
                default:
                    throw new IllegalArgumentException("无效的运算符");
                case '-':
                    return num1 - num2;
                case '/':
                    return num1 / num2;
            }
        }
    }

    private static Map<String, String> getBatchDeepSeekResponse(List<JsonNode> fields, Requests request) {
        try {
            String apiKey = "error:sk-eaa0008c8fa54b3c830d3095c2477f5c";
            if (apiKey != null && !apiKey.isEmpty()) {
                String aiRuleValue = null;

                try {
                    JsonNode predictionRules = objectMapper.readTree(request.getOrderTemplate());
                    if (predictionRules.isArray()) {
                        for(JsonNode rule : predictionRules) {
                            if ("AI规则".equals(rule.get("keyword").asText())) {
                                aiRuleValue = rule.get("value").asText();
                                break;
                            }
                        }
                    }
                } catch (Exception var27) {
                    logger.debug("未找到AI规则或解析失败");
                }

                StringBuilder fieldsPrompt = new StringBuilder();

                for(JsonNode field : fields) {
                    String fieldName = field.get("name").asText();
                    String fieldFormat = field.get("format").asText();
                    fieldsPrompt.append(String.format("字段名称：%s\n字段类型：%s\n---\n", fieldName, fieldFormat));
                }

                String prompt = String.format("你是一个表单填写助手，需要帮助填写以下表单项。\n\n%s\n待填写的表单项（每项用'---'分隔）：\n%s\n填写要求：\n1. 优先使用上述参考规则中的问题和答案进行匹配。如果找到相似的问题，直接使用对应的答案\n2. 如果参考规则中没有匹配的内容，再根据字段名称的语义填写合适的内容\n3. 对于数字类型(num)的字段，必须只返回纯数字\n4. 所有答案必须简短且符合实际场景\n5. 严格按照以下格式返回答案：\n字段名称|字段类型:答案\n\n示例格式：\n姓名|text:张三\n年龄|num:25\n", aiRuleValue != null ? "参考规则：\n" + aiRuleValue + "\n" : "", fieldsPrompt.toString());
                URL url = new URL("https://api.deepseek.com/chat/completions");
                HttpURLConnection conn = (HttpURLConnection)url.openConnection();
                conn.setRequestMethod("POST");
                conn.setRequestProperty("Content-Type", "application/json");
                conn.setRequestProperty("Authorization", "Bearer " + apiKey);
                conn.setDoOutput(true);
                ObjectNode requestBody = objectMapper.createObjectNode();
                requestBody.put("model", "deepseek-chat");
                requestBody.put("stream", false);
                requestBody.put("temperature", 0.2);
                requestBody.put("max_tokens", 1024);
                ArrayNode messages = requestBody.putArray("messages");
                ObjectNode systemMessage = objectMapper.createObjectNode();
                systemMessage.put("role", "system");
                systemMessage.put("content", "你是一个专业的表单填写助手。你的主要任务是：\n1. 优先从提供的参考规则中寻找匹配的问题和答案\n2. 只有在参考规则中找不到匹配内容时，才使用自己的知识填写\n3. 严格按照要求的格式返回答案\n" + (aiRuleValue != null ? "4. 当前任务提供了参考规则，你必须优先使用这些规则来生成答案" : ""));
                messages.add(systemMessage);
                ObjectNode userMessage = objectMapper.createObjectNode();
                userMessage.put("role", "user");
                userMessage.put("content", prompt);
                messages.add(userMessage);

                try (OutputStream os = conn.getOutputStream()) {
                    byte[] input = requestBody.toString().getBytes(StandardCharsets.UTF_8);
                    os.write(input, 0, input.length);
                }

                StringBuilder response = new StringBuilder();

                String responseLine;
                try (BufferedReader br = new BufferedReader(new InputStreamReader(conn.getInputStream(), StandardCharsets.UTF_8))) {
                    while((responseLine = br.readLine()) != null) {
                        response.append(responseLine.trim());
                    }
                }

                JsonNode responseJson = objectMapper.readTree(response.toString());
                responseLine = responseJson.get("choices").get(0).get("message").get("content").asText().trim();
                HashMap results = new HashMap();

                for(String line : responseLine.split("\n")) {
                    if (line.contains(":")) {
                        String[] parts = line.split(":", 2);
                        String fieldId = parts[0].trim();
                        String value = parts[1].trim();
                        if (fieldId.endsWith("|num") && !value.matches("\\d+")) {
                            value = value.replaceAll("[^0-9]", "");
                        }

                        if (!value.isEmpty()) {
                            results.put(fieldId, value);
                        }
                    }
                }

                return results;
            } else {
                logger.error("未找到DeepSeek API密钥");
                return Collections.emptyMap();
            }
        } catch (Exception e) {
            logger.error("调用DeepSeek API失败: {}", e.getMessage());
            return Collections.emptyMap();
        }
    }
}
