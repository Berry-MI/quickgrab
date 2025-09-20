/*
 * Decompiled with CFR 0.152.
 * 
 * Could not load the following classes:
 *  com.fasterxml.jackson.databind.JsonNode
 *  com.fasterxml.jackson.databind.node.ArrayNode
 *  com.fasterxml.jackson.databind.node.ObjectNode
 *  indi.wookat.quickgrab.util.FindUtil
 *  org.springframework.stereotype.Component
 */
package indi.wookat.quickgrab.util;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.node.ArrayNode;
import com.fasterxml.jackson.databind.node.ObjectNode;
import java.util.Iterator;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import org.springframework.stereotype.Component;

/*
 * Exception performing whole class analysis ignored.
 */
@Component
public class FindUtil {
    public static String findValueByKey(JsonNode rootNode, String key) {
        String result;
        if (rootNode.has(key)) {
            return rootNode.get(key).asText();
        }
        if (rootNode.isObject()) {
            for (JsonNode value : rootNode) {
                result = FindUtil.findValueByKey((JsonNode)value, (String)key);
                if (result == null) continue;
                return result;
            }
        }
        if (rootNode.isArray()) {
            for (JsonNode arrayItem : rootNode) {
                result = FindUtil.findValueByKey((JsonNode)arrayItem, (String)key);
                if (result == null) continue;
                return result;
            }
        }
        return null;
    }

    public static String findObjectElementStringValueByOtherValue(JsonNode jsonNode, String keyName, String targetValue, String elementToFind) {
        block4: {
            block3: {
                if (!jsonNode.isObject()) break block3;
                ObjectNode objectNode = (ObjectNode)jsonNode;
                JsonNode keyValueNode = objectNode.get(keyName);
                if (keyValueNode != null && keyValueNode.asText().equals(targetValue)) {
                    return FindUtil.findElementInNode((JsonNode)objectNode, (String)elementToFind);
                }
                Iterator fields = objectNode.fields();
                while (fields.hasNext()) {
                    Map.Entry entry = (Map.Entry)fields.next();
                    String foundValue = FindUtil.findObjectElementStringValueByOtherValue((JsonNode)((JsonNode)entry.getValue()), (String)keyName, (String)targetValue, (String)elementToFind);
                    if (foundValue == null) continue;
                    return foundValue;
                }
                break block4;
            }
            if (!jsonNode.isArray()) break block4;
            ArrayNode arrayNode = (ArrayNode)jsonNode;
            for (JsonNode item : arrayNode) {
                String foundValue = FindUtil.findObjectElementStringValueByOtherValue((JsonNode)item, (String)keyName, (String)targetValue, (String)elementToFind);
                if (foundValue == null) continue;
                return foundValue;
            }
        }
        return null;
    }

    private static String findElementInNode(JsonNode node, String elementPath) {
        String[] elements;
        for (String element : elements = elementPath.split("\\.")) {
            if (node.isObject()) {
                if ((node = node.get(element)) != null) continue;
                return null;
            }
            return null;
        }
        return node.asText();
    }

    public static String extractUserId(String query) {
        String userId = null;
        String pattern = "userid=([^&]*)";
        Pattern r = Pattern.compile(pattern);
        Matcher m = r.matcher(query);
        if (m.find()) {
            userId = m.group(1);
        }
        return userId;
    }
}

