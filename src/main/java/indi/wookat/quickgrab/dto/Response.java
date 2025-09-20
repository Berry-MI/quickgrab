/*
 * Decompiled with CFR 0.152.
 * 
 * Could not load the following classes:
 *  com.fasterxml.jackson.databind.JsonNode
 *  com.fasterxml.jackson.databind.ObjectMapper
 *  com.fasterxml.jackson.databind.node.ObjectNode
 *  indi.wookat.quickgrab.dto.Response
 */
package indi.wookat.quickgrab.dto;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.node.ObjectNode;

public record Response(JsonNode status, JsonNode result) {
//    private final JsonNode status;
//    private final JsonNode result;
    static ObjectMapper mapper = new ObjectMapper();
    static ObjectNode statusNode = mapper.createObjectNode();
    static ObjectNode resultNode = mapper.createObjectNode();

    public Response(JsonNode status, JsonNode result) {
        this.status = status;
        this.result = result;
    }

    public static Response createErrorResponse(int code, String description) {
        statusNode.put("code", code);
        statusNode.put("message", "ERROR");
        statusNode.put("description", description);
        return new Response((JsonNode)statusNode, (JsonNode)resultNode);
    }

    public static Response createSuccessResponse(int code, String message, JsonNode result) {
        statusNode.put("code", code);
        statusNode.put("message", message);
        return new Response((JsonNode)statusNode, result);
    }

    public JsonNode status() {
        return this.status;
    }

    public JsonNode result() {
        return this.result;
    }
}

