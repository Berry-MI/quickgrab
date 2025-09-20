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
import java.nio.charset.StandardCharsets;
import java.util.Date;
import java.util.Properties;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class MailService {
    private static final ObjectMapper mapper = new ObjectMapper();
    private static final String DEFAULT_FROM_EMAIL = "1966099953@qq.com";
    private static final String DEFAULT_PASSWORD = "fdmbzyrlwfmydhij";
    private static final String SENDER_NAME = "微店任务通知";
    private static final Logger logger = LoggerFactory.getLogger(MailService.class);
    private static final MailService DEFAULT_INSTANCE = new MailService(DEFAULT_FROM_EMAIL, DEFAULT_PASSWORD);
    private final String fromEmail;
    private final String password;
    private final Session session;
    private static final String EMAIL_TEMPLATE = "<div style='max-width: 600px; margin: 0 auto; font-family: Arial, sans-serif; line-height: 1.6; background-color: #f9f9f9; padding: 20px;'>\n    <div style='background-color: white; border-radius: 10px; padding: 30px; box-shadow: 0 2px 10px rgba(0,0,0,0.1);'>\n        <div style='text-align: center; margin-bottom: 30px;'>\n            <h1 style='color: %s; font-size: 24px; margin: 0;'>%s</h1>\n            <p style='color: #7f8c8d; font-size: 14px; margin-top: 5px;'>%s</p>\n        </div>\n        %s\n        <div style='margin: 25px 0; text-align: center;'>\n            <a href='%s' style='display: inline-block; padding: 12px 30px; font-size: 16px; color: #fff; background: %s; text-decoration: none; border-radius: 50px; box-shadow: 0 3px 6px %s; transition: transform 0.2s;'>\n                %s →\n            </a>\n        </div>\n        %s\n        <div style='margin-top: 30px; padding-top: 20px; border-top: 1px solid #eee; text-align: center; color: #95a5a6; font-size: 12px;'>\n            <p>%s</p>\n            <p style='margin: 5px 0;'>本邮件由系统自动发送，请勿回复</p>\n        </div>\n    </div>\n</div>\n";

    private record EmailTemplate(String themeColor, String title, String subtitle, String mainContent, String link,
                                 String buttonColor, String buttonShadow, String buttonText, String extraContent,
                                 String footerText) {
    }

    public MailService(String fromEmail, String password) {
        this.fromEmail = fromEmail;
        this.password = password;
        this.session = this.createMailSession();
    }

    public static boolean sendSuccesedEmail(Results grabResult) throws JsonProcessingException {
        if (grabResult == null || grabResult.getStatus() != 1) {
            return false;
        } else {
            String responseMessage = grabResult.getResponseMessage();
            if (responseMessage == null || responseMessage.isBlank()) {
                return false;
            }

            JsonNode responseBody = mapper.readTree(responseMessage);
            JsonNode payLink = responseBody.path("orderLink_list");
            if (payLink == null || !payLink.isArray() || payLink.isEmpty()) {
                return false;
            }

            String extension = grabResult.getExtension();
            if (extension == null || extension.isBlank()) {
                return false;
            }

            JsonNode extensionJson = mapper.readTree(extension);
            String email = extensionJson.path("email").asText("");
            boolean emailReminder = extensionJson.path("emailReminder").asBoolean(false);
            if (emailReminder && isValidEmail(email)) {
                String userInfoRaw = grabResult.getUserInfo();
                if (userInfoRaw == null || userInfoRaw.isBlank()) {
                    return false;
                }

                JsonNode userInfo = mapper.readTree(userInfoRaw);
                String telephone = userInfo.path("telephone").asText("");
                String nickName = userInfo.path("nickName").asText("");
                JsonNode firstLink = payLink.get(0);
                String link = firstLink.path("orderLink").asText("");
                String desc = firstLink.path("desc").asText("");
                sendEmailInfo(email, telephone + "(" + nickName + ")", link, desc);
                return true;
            }

            return false;
        }
    }

    public static void sendEmailInfo(String email, String phoneNumber, String link, String desc) {
        EmailTemplate template = new EmailTemplate(
                "#2ecc71",
                "✅ 下单成功",
                "请及时完成支付",
                createInfoBox("手机号", phoneNumber, "#2ecc71"),
                link,
                "linear-gradient(135deg, #2ecc71, #27ae60)",
                "rgba(46,204,113,0.3)",
                "立即支付",
                createDescriptionBox(desc),
                "如非本人操作，请忽略本通知"
        );
        defaultService().sendEmail(email, "微店下单成功通知", buildEmailContent(template));
    }

    public static void sendFailEmailInfo(Requests request, JsonNode responseBody) throws JsonProcessingException {
        JsonNode extensionNode = parseOrEmpty(request.getExtension());
        JsonNode userInfoNode = parseOrEmpty(request.getUserInfo());
        String toEmail = extensionNode.path("email").asText(DEFAULT_FROM_EMAIL);
        if (toEmail.isEmpty() || "null".equalsIgnoreCase(toEmail)) {
            toEmail = DEFAULT_FROM_EMAIL;
        }

        String mainContent = createErrorBox(responseBody.path("reason").asText("未知异常")) +
                createInfoBox("手机号", userInfoNode.path("telephone").asText(""), "#3498db");
        EmailTemplate template = new EmailTemplate(
                "#e74c3c",
                "⚠️ 订单异常",
                "请查看详细信息",
                mainContent,
                request.getLink(),
                "linear-gradient(135deg, #3498db, #2980b9)",
                "rgba(52,152,219,0.3)",
                "查看详情",
                "",
                "如需帮助，请联系客服"
        );
        defaultService().sendEmail(toEmail, "微店订单异常通知", buildEmailContent(template));
    }

    public static void sendFailedGrabEmail(Results grabResult) throws JsonProcessingException {
        String extension = grabResult.getExtension();
        if (extension == null || extension.isBlank()) {
            return;
        }

        JsonNode extensionJson = mapper.readTree(extension);
        String email = extensionJson.path("email").asText("");
        boolean emailReminder = extensionJson.path("emailReminder").asBoolean(false);
        if (!emailReminder || !isValidEmail(email)) {
            return;
        }

        JsonNode userInfo = parseOrEmpty(grabResult.getUserInfo());
        String telephone = userInfo.path("telephone").asText("");
        String nickName = userInfo.path("nickName").asText("");
        JsonNode responseBody = parseOrEmpty(grabResult.getResponseMessage());
        String errorMessage = responseBody.has("error_message") ? responseBody.get("error_message").asText() : "抢购失败，请检查商品状态";
        String mainContent = createErrorBox(errorMessage) +
                createInfoBox("手机号", telephone + "(" + nickName + ")", "#3498db");
        EmailTemplate template = new EmailTemplate(
                "#e74c3c",
                "❌ 抢购失败",
                "请查看失败原因",
                mainContent,
                grabResult.getLink(),
                "linear-gradient(135deg, #3498db, #2980b9)",
                "rgba(52,152,219,0.3)",
                "查看详情",
                "",
                "如需帮助，请联系客服"
        );
        defaultService().sendEmail(email, "微店抢购失败通知", buildEmailContent(template));
    }

    public static void sendFoundItemEmailInfo(String email, String phoneNumber, String link, String itemTitle, String skuTitle) {
        EmailTemplate template = new EmailTemplate(
                "#1890ff",
                "🔍 找到商品",
                "已找到符合条件的商品",
                createInfoBox("手机号", phoneNumber, "#1890ff"),
                link,
                "linear-gradient(135deg, #1890ff, #096dd9)",
                "rgba(24,144,255,0.3)",
                "查看商品",
                createDescriptionBox("商品名称: " + itemTitle + "\n规格信息: " + skuTitle),
                "如非本人操作，请忽略本通知"
        );
        defaultService().sendEmail(email, "找到符合条件的商品", buildEmailContent(template));
    }

    public static boolean sendFoundItemEmail(Requests request, String link) throws JsonProcessingException {
        JsonNode extensionJson = parseOrEmpty(request.getExtension());
        String email = extensionJson.path("email").asText("");
        boolean emailReminder = extensionJson.path("emailReminder").asBoolean(false);
        if (!emailReminder || !isValidEmail(email)) {
            return false;
        }

        JsonNode userInfo = parseOrEmpty(request.getUserInfo());
        String telephone = userInfo.path("telephone").asText("");
        String nickName = userInfo.path("nickName").asText("");
        String titleContent = request.getKeyword() == null ? "" : request.getKeyword();
        String[] titleContents = titleContent.split("\\|", 2);
        String itemTitle = titleContents.length > 0 ? titleContents[0] : "";
        String skuTitle = titleContents.length > 1 ? titleContents[1] : "";
        sendFoundItemEmailInfo(email, telephone + "(" + nickName + ")", link, itemTitle, skuTitle);
        return true;
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
        Properties properties = new Properties();
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
            message.setFrom(new InternetAddress(this.fromEmail, SENDER_NAME, StandardCharsets.UTF_8.name()));
            message.addRecipient(RecipientType.TO, new InternetAddress(toEmail));
            message.setSubject(subject);
            message.setContent(body, "text/html;charset=UTF-8");
            message.setSentDate(new Date());
            Transport.send(message);
            logger.info("邮件发送成功：{}", toEmail);
        } catch (Exception e) {
            logger.error("邮件发送失败：{}", e.getMessage(), e);
        }

    }

    private static String buildEmailContent(EmailTemplate template) {
        return String.format(EMAIL_TEMPLATE,
                template.themeColor,
                template.title,
                template.subtitle,
                template.mainContent,
                template.link,
                template.buttonColor,
                template.buttonShadow,
                template.buttonText,
                template.extraContent,
                template.footerText);
    }

    private static boolean isValidEmail(String email) {
        return email != null && !email.isBlank() && email.contains("@");
    }

    private static MailService defaultService() {
        return DEFAULT_INSTANCE;
    }

    private static JsonNode parseOrEmpty(String json) throws JsonProcessingException {
        if (json == null || json.isBlank()) {
            return mapper.createObjectNode();
        }

        return mapper.readTree(json);
    }
}
