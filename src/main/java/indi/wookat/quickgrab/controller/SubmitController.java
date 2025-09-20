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
import indi.wookat.quickgrab.service.GrabService;
import indi.wookat.quickgrab.util.CommonUtil;
import indi.wookat.quickgrab.util.NetworkUtil;
import io.swagger.v3.oas.annotations.tags.Tag;
import jakarta.annotation.Resource;
import java.util.Random;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

@Tag(
        name = "控制器：提交",
        description = "描述：提交请求的控制器"
)
@RestController
@RequestMapping({"/api"})
public class SubmitController {
    private static final Logger logger = LoggerFactory.getLogger(SubmitController.class);
    ObjectMapper mapper = new ObjectMapper();
    ObjectNode resultNode;
    @Resource
    private GrabService grabService;

    public SubmitController() {
        this.resultNode = this.mapper.createObjectNode();
    }

    @PostMapping({"/submitRequest"})
    public Response submitRequest(@RequestBody Requests request) {
        try {
            JsonNode userInfoNode = NetworkUtil.getUserInfo(request.getCookies());
            if (userInfoNode == null) {
                return Response.createErrorResponse(202, "获取用户信息失败");
            } else {
                if (request.getUserInfo() != null && !request.getUserInfo().isEmpty()) {
                    ObjectNode mergedInfo = (ObjectNode)userInfoNode;
                    mergedInfo.setAll((ObjectNode)this.mapper.readTree(request.getUserInfo()));
                    request.setUserInfo(mergedInfo.toString());
                } else {
                    request.setUserInfo(userInfoNode.toString());
                }

                logger.info("用户信息：{}", request.getUserInfo());
                Random random = new Random();
                request.setDeviceId(random.nextInt(1) + 1);
                logger.info("设备信息：" + request.getDeviceId());
                this.resultNode.put("networkDelay", 0);
                if (request.getType() != 2) {
                    JsonNode dataObj = NetworkUtil.getAddOrderData(request);
                    if (dataObj == null) {
                        return Response.createErrorResponse(203, "获取订单信息失败");
                    }

                    JsonNode orderParameters = CommonUtil.generateOrderParameters(request, dataObj, true);
                    request.setOrderParameters(orderParameters.toString());
                    JsonNode result = NetworkUtil.sendReConfirmOrderRequest(request);
                    if (result == null) {
                        return Response.createErrorResponse(204, "确认订单失败");
                    }

                    long networkDelay = CommonUtil.getAdjustedFactor(request);
                    logger.info("网络延迟：{}", networkDelay);
                    this.resultNode.put("networkDelay", networkDelay);
                }

                this.resultNode.putPOJO("request", request);
                boolean success = this.grabService.handleRequest(request);
                return success ? Response.createSuccessResponse(200, "OK", this.resultNode) : Response.createErrorResponse(205, "插入请求失败");
            }
        } catch (Throwable $ex) {
            return Response.createErrorResponse(500, "服务器内部错误");
        }
    }
}
