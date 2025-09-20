//
// Source code recreated from a .class file by IntelliJ IDEA
// (powered by FernFlower decompiler)
//

package indi.wookat.quickgrab.controller;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.node.ObjectNode;
import indi.wookat.quickgrab.dto.Response;
import indi.wookat.quickgrab.entity.Requests;
import indi.wookat.quickgrab.service.QueryService;
import indi.wookat.quickgrab.util.CommonUtil;
import indi.wookat.quickgrab.util.NetworkUtil;
import jakarta.annotation.Resource;
import java.util.Collections;
import java.util.Map;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RestController;

@RestController
public class ToolController {
    @Resource
    private QueryService queryService;
    private ObjectMapper mapper = new ObjectMapper();
    private ObjectNode resultNode;
    private static final Logger logger = LoggerFactory.getLogger(ToolController.class);

    public ToolController() {
        this.resultNode = this.mapper.createObjectNode();
    }

    @PostMapping({"/getNote"})
    public Response getNote(@RequestBody Requests request) {
        try {
            logger.info("开始获取备注信息");
            JsonNode dataObj = NetworkUtil.getAddOrderData(request);
            logger.info("获取订单信息");
            if (dataObj == null) {
                return Response.createErrorResponse(400, "获取订单信息失败");
            } else {
                JsonNode orderParameters = CommonUtil.generateOrderParameters(request, dataObj, true);
                request.setOrderParameters(orderParameters.toString());
                JsonNode result = NetworkUtil.sendReConfirmOrderRequest(request);
                JsonNode customInfo = this.mapper.createObjectNode();
                if (result != null && result.has("custom_info")) {
                    customInfo = CommonUtil.processCustomInfo(result.get("custom_info"));
                }

                this.resultNode.set("custom_info", customInfo);
                logger.info("获取备注信息成功");
                return Response.createSuccessResponse(200, "OK", this.resultNode);
            }
        } catch (Exception e) {
            e.printStackTrace();
            return Response.createErrorResponse(500, e.getMessage());
        }
    }

    @PostMapping({"/fetchItemInfo"})
    public Response fetchItemInfo(@RequestParam(name = "link") String link, @RequestParam(name = "cookies") String cookies) {
        try {
            ObjectMapper mapper = new ObjectMapper();
            ObjectNode resultNode = mapper.createObjectNode();
            JsonNode paramNode = CommonUtil.convertLinkToJson(link);
            if (paramNode == null) {
                return Response.createErrorResponse(201, "获取参数失败");
            } else {
                System.out.println("link: " + link);
                System.out.println("paramNode: " + paramNode);
                String itemId = paramNode.get("item_list").get(0).get("item_id").asText();
                System.out.println("商品ID：" + itemId);
                JsonNode dataObj = NetworkUtil.getItemInfo(itemId, cookies);
                System.out.println(dataObj);
                if (dataObj == null) {
                    return Response.createErrorResponse(202, "获取商品信息失败");
                } else {
                    long skuId = paramNode.get("item_list").get(0).get("item_sku_id").asLong();
                    int stockQuantity;
                    if (skuId != 0L) {
                        JsonNode skuList = dataObj.get("skuProperties").get("sku");
                        stockQuantity = -1;

                        for(JsonNode sku : skuList) {
                            if (sku.get("id").asLong() == skuId) {
                                stockQuantity = sku.get("stock").asInt();
                                break;
                            }
                        }

                        if (stockQuantity == -1) {
                            return Response.createErrorResponse(203, "未找到对应的SKU信息");
                        }
                    } else {
                        stockQuantity = dataObj.get("itemInfo").get("stock").asInt();
                    }

                    int delayIncrement = this.calculateDelayIncrementWithJitter(stockQuantity);
                    resultNode.put("delayIncrement", delayIncrement);
                    System.out.println("延时增量：" + delayIncrement);
                    JsonNode ticketItemInfo = dataObj.get("itemInfo").get("ticketItemInfo");
                    if (ticketItemInfo != null) {
                        boolean hasExpireDate = ticketItemInfo.has("expireDate");
                        resultNode.put("hasExpireDate", hasExpireDate);
                        if (hasExpireDate) {
                            String expireDate = ticketItemInfo.get("expireDate").asText();
                            JsonNode item_convey_info = CommonUtil.parseExpireDate(expireDate);
                            resultNode.set("expireDate", item_convey_info);
                        }
                    }

                    boolean isFutureSold = dataObj.get("itemInfo").get("flag").get("isFutureSold").asBoolean();
                    resultNode.put("isFutureSold", isFutureSold);
                    if (isFutureSold) {
                        long futureSoldTime = dataObj.get("itemInfo").get("futureSoldTime").asLong();
                        resultNode.put("futureSoldTime", futureSoldTime);
                    }

                    int categoryCount = paramNode.get("item_list").size();
                    resultNode.put("categoryCount", categoryCount);
                    return Response.createSuccessResponse(200, "OK", resultNode);
                }
            }
        } catch (Exception e) {
            e.printStackTrace();
            return Response.createErrorResponse(204, e.getMessage());
        }
    }

    @GetMapping({"/checkCookiesValidity"})
    public ResponseEntity<Map<String, String>> checkCookiesValidity(@RequestParam String cookies) {
        boolean isValid = this.queryService.checkCookies(cookies);
        return ResponseEntity.ok(Collections.singletonMap("message", isValid ? "Cookies有效" : "Cookies无效"));
    }

    @PostMapping({"/checkLatency"})
    public long checkLatency(@RequestBody Requests request) {
        JsonNode dataObj = NetworkUtil.getAddOrderData(request);
        if (dataObj == null) {
            return -1L;
        } else {
            request.setOrderParameters(String.valueOf(CommonUtil.generateOrderParameters(request, dataObj, true)));
            JsonNode result = NetworkUtil.sendReConfirmOrderRequest(request);
            return result == null ? -1L : result.get("networkDaly").asLong();
        }
    }

    private int calculateDelayIncrementWithJitter(int stockQuantity) {
        if (stockQuantity < 5) {
            return 0;
        } else {
            double baseDelay = Math.log10((double)stockQuantity) * 1.05;
            double jitterRange = Math.min(baseDelay * 0.2, 0.3);
            double randomJitter = (Math.random() * (double)2.0F - (double)1.0F) * jitterRange;
            return (int)Math.min(Math.max(Math.round(baseDelay + randomJitter), 0L), 3L);
        }
    }
}
