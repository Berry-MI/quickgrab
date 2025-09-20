//
// Source code recreated from a .class file by IntelliJ IDEA
// (powered by FernFlower decompiler)
//

package indi.wookat.quickgrab.service;

import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import indi.wookat.quickgrab.entity.Requests;
import indi.wookat.quickgrab.entity.Results;
import jakarta.mail.Authenticator;
import jakarta.mail.PasswordAuthentication;
import jakarta.mail.Session;
import jakarta.mail.Transport;
import jakarta.mail.Message.RecipientType;
import jakarta.mail.internet.InternetAddress;
import jakarta.mail.internet.MimeMessage;
import java.util.Properties;

public class MailService {
    private static final ObjectMapper mapper = new ObjectMapper();
    private static final String DEFAULT_FROM_EMAIL = "1966099953@qq.com";
    private static final String DEFAULT_PASSWORD = "fdmbzyrlwfmydhij";
    private static final String SENDER_NAME = "微店任务通知";
    private final String fromEmail;
    private final String password;
    private final Session session;
    private static final String EMAIL_TEMPLATE = "<div style='max-width: 600px; margin: 0 auto; font-family: Arial, sans-serif; line-height: 1.6; background-color: #f9f9f9; padding: 20px;'>\n    <div style='background-color: white; border-radius: 10px; padding: 30px; box-shadow: 0 2px 10px rgba(0,0,0,0.1);'>\n        <div style='text-align: center; margin-bottom: 30px;'>\n            <h1 style='color: %s; font-size: 24px; margin: 0;'>%s</h1>\n            <p style='color: #7f8c8d; font-size: 14px; margin-top: 5px;'>%s</p>\n        </div>\n        %s\n        <div style='margin: 25px 0; text-align: center;'>\n            <a href='%s' style='display: inline-block; padding: 12px 30px; font-size: 16px; color: #fff; background: %s; text-decoration: none; border-radius: 50px; box-shadow: 0 3px 6px %s; transition: transform 0.2s;'>\n                %s →\n            </a>\n        </div>\n        %s\n        <div style='margin-top: 30px; padding-top: 20px; border-top: 1px solid #eee; text-align: center; color: #95a5a6; font-size: 12px;'>\n            <p>%s</p>\n            <p style='margin: 5px 0;'>本邮件由系统自动发送，请勿回复</p>\n        </div>\n    </div>\n</div>\n";

    public MailService(String fromEmail, String password) {
        this.fromEmail = fromEmail;
        this.password = password;
        this.session = this.createMailSession();
    }

    public static boolean sendSuccesedEmail(Results grabResult) throws JsonProcessingException {
        if (grabResult.getStatus() != 1) {
            return false;
        } else {
            JsonNode responseBody = mapper.readTree(grabResult.getResponseMessage());
            JsonNode payLink = responseBody.has("orderLink_list") ? responseBody.get("orderLink_list") : null;
            JsonNode extensionJson = mapper.readTree(grabResult.getExtension());
            String email = extensionJson.get("email").asText();
            boolean emailReminder = extensionJson.get("emailReminder").asBoolean();
            if (emailReminder && email.contains("@") && payLink != null && payLink.size() != 0) {
                JsonNode userInfo = mapper.readTree(grabResult.getUserInfo());
                String telephone = userInfo.get("telephone").asText();
                String nickName = userInfo.get("nickName").asText();
                String link = payLink.get(0).get("orderLink").asText();
                String desc = payLink.get(0).get("desc").asText();
                sendEmailInfo(email, telephone + "(" + nickName + ")", link, desc);
                return true;
            } else {
                return false;
            }
        }
    }

    public static void sendEmailInfo(String email, String phoneNumber, String link, String desc) {
        MailService mailService = new MailService("1966099953@qq.com", "fdmbzyrlwfmydhij");
        String content = String.format("<div style='max-width: 600px; margin: 0 auto; font-family: Arial, sans-serif; line-height: 1.6; background-color: #f9f9f9; padding: 20px;'>\n    <div style='background-color: white; border-radius: 10px; padding: 30px; box-shadow: 0 2px 10px rgba(0,0,0,0.1);'>\n        <div style='text-align: center; margin-bottom: 30px;'>\n            <h1 style='color: %s; font-size: 24px; margin: 0;'>%s</h1>\n            <p style='color: #7f8c8d; font-size: 14px; margin-top: 5px;'>%s</p>\n        </div>\n        %s\n        <div style='margin: 25px 0; text-align: center;'>\n            <a href='%s' style='display: inline-block; padding: 12px 30px; font-size: 16px; color: #fff; background: %s; text-decoration: none; border-radius: 50px; box-shadow: 0 3px 6px %s; transition: transform 0.2s;'>\n                %s →\n            </a>\n        </div>\n        %s\n        <div style='margin-top: 30px; padding-top: 20px; border-top: 1px solid #eee; text-align: center; color: #95a5a6; font-size: 12px;'>\n            <p>%s</p>\n            <p style='margin: 5px 0;'>本邮件由系统自动发送，请勿回复</p>\n        </div>\n    </div>\n</div>\n", "#2ecc71", "✅ 下单成功", "请及时完成支付", createInfoBox("手机号", phoneNumber, "#2ecc71"), link, "linear-gradient(135deg, #2ecc71, #27ae60)", "rgba(46,204,113,0.3)", "立即支付", createDescriptionBox(desc), "如非本人操作，请忽略本通知");
        mailService.sendEmail(email, "微店下单成功通知", content);
    }

    public static void sendFailEmailInfo(Requests request, JsonNode responseBody) throws JsonProcessingException {
        MailService mailService = new MailService("1966099953@qq.com", "fdmbzyrlwfmydhij");
        JsonNode extensionNode = mapper.readTree(request.getExtension());
        JsonNode userInfoNode = mapper.readTree(request.getUserInfo());
        String toEmail = extensionNode.get("email").asText();
        toEmail = !toEmail.isEmpty() && !"null".equals(toEmail) ? toEmail : "1966099953@qq.com";
        Object[] var10001 = new Object[]{"#e74c3c", "⚠️ 订单异常", "请查看详细信息", null, null, null, null, null, null, null};
        String var10004 = createErrorBox(responseBody.get("reason").asText());
        var10001[3] = var10004 + createInfoBox("手机号", userInfoNode.get("telephone").asText(), "#3498db");
        var10001[4] = request.getLink();
        var10001[5] = "linear-gradient(135deg, #3498db, #2980b9)";
        var10001[6] = "rgba(52,152,219,0.3)";
        var10001[7] = "查看详情";
        var10001[8] = "";
        var10001[9] = "如需帮助，请联系客服";
        String content = String.format("<div style='max-width: 600px; margin: 0 auto; font-family: Arial, sans-serif; line-height: 1.6; background-color: #f9f9f9; padding: 20px;'>\n    <div style='background-color: white; border-radius: 10px; padding: 30px; box-shadow: 0 2px 10px rgba(0,0,0,0.1);'>\n        <div style='text-align: center; margin-bottom: 30px;'>\n            <h1 style='color: %s; font-size: 24px; margin: 0;'>%s</h1>\n            <p style='color: #7f8c8d; font-size: 14px; margin-top: 5px;'>%s</p>\n        </div>\n        %s\n        <div style='margin: 25px 0; text-align: center;'>\n            <a href='%s' style='display: inline-block; padding: 12px 30px; font-size: 16px; color: #fff; background: %s; text-decoration: none; border-radius: 50px; box-shadow: 0 3px 6px %s; transition: transform 0.2s;'>\n                %s →\n            </a>\n        </div>\n        %s\n        <div style='margin-top: 30px; padding-top: 20px; border-top: 1px solid #eee; text-align: center; color: #95a5a6; font-size: 12px;'>\n            <p>%s</p>\n            <p style='margin: 5px 0;'>本邮件由系统自动发送，请勿回复</p>\n        </div>\n    </div>\n</div>\n", var10001);
        mailService.sendEmail(toEmail, "微店订单异常通知", content);
    }

    public static void sendFailedGrabEmail(Results grabResult) throws JsonProcessingException {
        MailService mailService = new MailService("1966099953@qq.com", "fdmbzyrlwfmydhij");
        JsonNode extensionJson = mapper.readTree(grabResult.getExtension());
        String email = extensionJson.get("email").asText();
        boolean emailReminder = extensionJson.get("emailReminder").asBoolean();
        if (emailReminder && email.contains("@")) {
            JsonNode userInfo = mapper.readTree(grabResult.getUserInfo());
            String telephone = userInfo.get("telephone").asText();
            String nickName = userInfo.get("nickName").asText();
            JsonNode responseBody = mapper.readTree(grabResult.getResponseMessage());
            String errorMessage = responseBody.has("error_message") ? responseBody.get("error_message").asText() : "抢购失败，请检查商品状态";
            Object[] var10001 = new Object[]{"#e74c3c", "❌ 抢购失败", "请查看失败原因", null, null, null, null, null, null, null};
            String var10004 = createErrorBox(errorMessage);
            var10001[3] = var10004 + createInfoBox("手机号", telephone + "(" + nickName + ")", "#3498db");
            var10001[4] = grabResult.getLink();
            var10001[5] = "linear-gradient(135deg, #3498db, #2980b9)";
            var10001[6] = "rgba(52,152,219,0.3)";
            var10001[7] = "查看详情";
            var10001[8] = "";
            var10001[9] = "如需帮助，请联系客服";
            String content = String.format("<div style='max-width: 600px; margin: 0 auto; font-family: Arial, sans-serif; line-height: 1.6; background-color: #f9f9f9; padding: 20px;'>\n    <div style='background-color: white; border-radius: 10px; padding: 30px; box-shadow: 0 2px 10px rgba(0,0,0,0.1);'>\n        <div style='text-align: center; margin-bottom: 30px;'>\n            <h1 style='color: %s; font-size: 24px; margin: 0;'>%s</h1>\n            <p style='color: #7f8c8d; font-size: 14px; margin-top: 5px;'>%s</p>\n        </div>\n        %s\n        <div style='margin: 25px 0; text-align: center;'>\n            <a href='%s' style='display: inline-block; padding: 12px 30px; font-size: 16px; color: #fff; background: %s; text-decoration: none; border-radius: 50px; box-shadow: 0 3px 6px %s; transition: transform 0.2s;'>\n                %s →\n            </a>\n        </div>\n        %s\n        <div style='margin-top: 30px; padding-top: 20px; border-top: 1px solid #eee; text-align: center; color: #95a5a6; font-size: 12px;'>\n            <p>%s</p>\n            <p style='margin: 5px 0;'>本邮件由系统自动发送，请勿回复</p>\n        </div>\n    </div>\n</div>\n", var10001);
            mailService.sendEmail(email, "微店抢购失败通知", content);
        }
    }

    public static void sendFoundItemEmailInfo(String email, String phoneNumber, String link, String itemTitle, String skuTitle) {
        MailService mailService = new MailService("1966099953@qq.com", "fdmbzyrlwfmydhij");
        String content = String.format("<div style='max-width: 600px; margin: 0 auto; font-family: Arial, sans-serif; line-height: 1.6; background-color: #f9f9f9; padding: 20px;'>\n    <div style='background-color: white; border-radius: 10px; padding: 30px; box-shadow: 0 2px 10px rgba(0,0,0,0.1);'>\n        <div style='text-align: center; margin-bottom: 30px;'>\n            <h1 style='color: %s; font-size: 24px; margin: 0;'>%s</h1>\n            <p style='color: #7f8c8d; font-size: 14px; margin-top: 5px;'>%s</p>\n        </div>\n        %s\n        <div style='margin: 25px 0; text-align: center;'>\n            <a href='%s' style='display: inline-block; padding: 12px 30px; font-size: 16px; color: #fff; background: %s; text-decoration: none; border-radius: 50px; box-shadow: 0 3px 6px %s; transition: transform 0.2s;'>\n                %s →\n            </a>\n        </div>\n        %s\n        <div style='margin-top: 30px; padding-top: 20px; border-top: 1px solid #eee; text-align: center; color: #95a5a6; font-size: 12px;'>\n            <p>%s</p>\n            <p style='margin: 5px 0;'>本邮件由系统自动发送，请勿回复</p>\n        </div>\n    </div>\n</div>\n", "#1890ff", "\ud83d\udd0d 找到商品", "已找到符合条件的商品", createInfoBox("手机号", phoneNumber, "#1890ff"), link, "linear-gradient(135deg, #1890ff, #096dd9)", "rgba(24,144,255,0.3)", "查看商品", createDescriptionBox("商品名称: " + itemTitle + "\n规格信息: " + skuTitle), "如非本人操作，请忽略本通知");
        mailService.sendEmail(email, "找到符合条件的商品", content);
    }

    public static boolean sendFoundItemEmail(Requests request, String link) throws JsonProcessingException {
        JsonNode extensionJson = mapper.readTree(request.getExtension());
        String email = extensionJson.get("email").asText();
        boolean emailReminder = extensionJson.get("emailReminder").asBoolean();
        if (emailReminder && email.contains("@")) {
            JsonNode userInfo = mapper.readTree(request.getUserInfo());
            String telephone = userInfo.get("telephone").asText();
            String nickName = userInfo.get("nickName").asText();
            String[] titleContents = request.getKeyword().split("\\|", 2);
            String itemTitle = titleContents.length > 0 ? titleContents[0] : "";
            String skuTitle = titleContents.length > 1 ? titleContents[1] : "";
            sendFoundItemEmailInfo(email, telephone + "(" + nickName + ")", link, itemTitle, skuTitle);
            return true;
        } else {
            return false;
        }
    }

    private static String createInfoBox(String label, String value, String color) {
        return String.format("<div style='background-color: #f8f9fa; border-left: 4px solid %s; padding: 15px; margin-bottom: 20px;'>\n    <p style='margin: 0; font-size: 15px; color: #2c3e50;'>\n        <strong>%s：</strong>\n        <span style='color: #34495e;'>%s</span>\n    </p>\n</div>\n", color, label, value);
    }

    private static String createErrorBox(String reason) {
        return String.format("<div style='background-color: #fff5f5; border-left: 4px solid #e74c3c; padding: 15px; margin-bottom: 20px;'>\n    <p style='margin: 0; font-size: 15px; color: #c0392b;'>\n        <strong>异常原因：</strong>\n        <span>%s</span>\n    </p>\n</div>\n", reason);
    }

    private static String createDescriptionBox(String desc) {
        return String.format("<div style='background-color: #fff8f0; border-radius: 8px; padding: 15px; margin-top: 20px;'>\n    <p style='margin: 0; color: #e67e22; font-size: 14px;'>\n        <span style='font-weight: bold;'>\ud83d\udcdd 订单说明：</span>\n        %s\n    </p>\n</div>\n", desc);
    }

    private Session createMailSession() {
        Properties properties = System.getProperties();
        properties.setProperty("mail.smtp.host", "smtp.qq.com");
        properties.setProperty("mail.smtp.port", "465");
        properties.setProperty("mail.smtp.auth", "true");
        properties.setProperty("mail.smtp.ssl.enable", "true");
        properties.setProperty("mail.smtp.ssl.protocols", "TLSv1.2");
        return Session.getInstance(properties, new Authenticator() {
            protected PasswordAuthentication getPasswordAuthentication() {
                return new PasswordAuthentication(MailService.this.fromEmail, MailService.this.password);
            }
        });
    }

    private void sendEmail(String toEmail, String subject, String body) {
        try {
            MimeMessage message = new MimeMessage(this.session);
            message.setFrom(new InternetAddress(this.fromEmail, "微店任务通知"));
            message.addRecipient(RecipientType.TO, new InternetAddress(toEmail));
            message.setSubject(subject);
            message.setContent(body, "text/html;charset=UTF-8");
            Transport.send(message);
            System.out.println("邮件发送成功：" + toEmail);
        } catch (Exception e) {
            System.err.println("邮件发送失败：" + e.getMessage());
            e.printStackTrace();
        }

    }
}
