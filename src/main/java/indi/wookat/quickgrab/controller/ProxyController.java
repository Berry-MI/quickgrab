//
// Source code recreated from a .class file by IntelliJ IDEA
// (powered by FernFlower decompiler)
//

package indi.wookat.quickgrab.controller;

import com.fasterxml.jackson.databind.JsonNode;
import indi.wookat.quickgrab.util.NetworkUtil;
import java.io.IOException;
import java.net.URI;
import java.net.URLEncoder;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.net.http.HttpClient.Redirect;
import java.net.http.HttpRequest.BodyPublishers;
import java.net.http.HttpResponse.BodyHandlers;
import java.nio.charset.StandardCharsets;
import org.springframework.core.io.ByteArrayResource;
import org.springframework.http.HttpEntity;
import org.springframework.http.HttpHeaders;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.util.LinkedMultiValueMap;
import org.springframework.util.MultiValueMap;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RestController;
import org.springframework.web.client.RestTemplate;
import org.springframework.web.multipart.MultipartFile;

@RestController
@RequestMapping({"/api"})
public class ProxyController {
    @PostMapping({"/upload"})
    public String uploadImage(@RequestParam("file") final MultipartFile file, @RequestParam("customCookies") String customCookies) throws IOException {
        String targetUrl = "https://vimg.weidian.com/upload/v3/direct?scope=addorder&fileType=image";
        RestTemplate restTemplate = new RestTemplate();
        HttpHeaders headers = new HttpHeaders();
        headers.add("Cookie", customCookies);
        headers.add("Referer", "https://weidian.com/");
        headers.add("User-Agent", "Mozilla/5.0 (iPhone; CPU iPhone OS 16_6 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/16.6 Mobile/15E148 Safari/604.1 Edg/122.0.0.0");
        MultiValueMap<String, Object> body = new LinkedMultiValueMap();
        body.add("file", new ByteArrayResource(file.getBytes()) {
            public String getFilename() {
                return file.getOriginalFilename();
            }
        });
        body.add("unadjust", false);
        body.add("prv", false);
        HttpEntity<MultiValueMap<String, Object>> requestEntity = new HttpEntity(body, headers);
        ResponseEntity<String> response = restTemplate.postForEntity(targetUrl, requestEntity, String.class, new Object[0]);
        return (String)response.getBody();
    }

    @GetMapping({"/expand"})
    public ResponseEntity<String> expandShortUrl(@RequestParam String shortUrl) {
        try {
            HttpClient httpClient = HttpClient.newBuilder().followRedirects(Redirect.ALWAYS).build();
            HttpRequest httpRequest = HttpRequest.newBuilder().uri(new URI(shortUrl)).header("Referer", "https://weidian.com/").GET().build();
            HttpResponse<Void> response = httpClient.send(httpRequest, BodyHandlers.discarding());
            String originalLink = response.uri().toString();
            return ResponseEntity.ok(originalLink);
        } catch (Exception e) {
            e.printStackTrace();
            return ResponseEntity.status(500).body("Failed to restore the original link: " + e.getMessage());
        }
    }

    @GetMapping({"/getItemSkuInfo"})
    public ResponseEntity<String> proxyRequest(@RequestParam String param) {
        try {
            String encodedParam = URLEncoder.encode(param, StandardCharsets.UTF_8.toString());
            String url = "https://thor.weidian.com/detail/getItemSkuInfo/1.0?param=" + encodedParam;
            HttpClient httpClient = HttpClient.newBuilder().followRedirects(Redirect.NORMAL).build();
            HttpRequest request = HttpRequest.newBuilder().uri(new URI(url)).header("Referer", "https://weidian.com/").header("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/108.0.0.0 Safari/537.36 Edg/108.0.1462.76").GET().build();
            HttpResponse<String> response = httpClient.send(request, BodyHandlers.ofString());
            HttpHeaders headers = new HttpHeaders();
            headers.add("Content-Type", "application/json");
            return new ResponseEntity((String)response.body(), headers, HttpStatus.OK);
        } catch (Exception e) {
            e.printStackTrace();
            return new ResponseEntity(HttpStatus.INTERNAL_SERVER_ERROR);
        }
    }

    @PostMapping({"/loginbyvcode"})
    public ResponseEntity<String> loginByVcode(@RequestParam("phone") String phone, @RequestParam("countryCode") String countryCode, @RequestParam("vcode") String vcode) {
        try {
            String url = "https://sso.weidian.com/user/loginbyvcode";
            String requestBody = String.format("phone=%s&countryCode=%s&vcode=%s", URLEncoder.encode(phone, StandardCharsets.UTF_8), URLEncoder.encode(countryCode, StandardCharsets.UTF_8), URLEncoder.encode(vcode, StandardCharsets.UTF_8));
            HttpClient httpClient = HttpClient.newBuilder().followRedirects(Redirect.NORMAL).build();
            HttpRequest request = HttpRequest.newBuilder().uri(new URI(url)).header("Content-Type", "application/x-www-form-urlencoded").header("Referer", "https://weidian.com/").header("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/108.0.0.0 Safari/537.36").POST(BodyPublishers.ofString(requestBody)).build();
            HttpResponse<String> response = httpClient.send(request, BodyHandlers.ofString());
            HttpHeaders headers = new HttpHeaders();
            headers.add("Content-Type", "application/json");
            return new ResponseEntity((String)response.body(), headers, HttpStatus.valueOf(response.statusCode()));
        } catch (Exception e) {
            e.printStackTrace();
            return new ResponseEntity("Login request failed: " + e.getMessage(), HttpStatus.INTERNAL_SERVER_ERROR);
        }
    }

    @PostMapping({"/getListCart"})
    public ResponseEntity<String> getListCart(@RequestParam("cookie") String cookie) {
        try {
            String url = "https://thor.weidian.com/vcart/getListCart/3.0";
            String param = "{\"source\":\"h5\",\"v_seller_id\":\"\",\"tabKey\":\"all\"}";
            String encodedParam = URLEncoder.encode(param, StandardCharsets.UTF_8);
            String requestBody = "param=" + encodedParam;
            HttpClient httpClient = HttpClient.newBuilder().followRedirects(Redirect.NORMAL).build();
            HttpRequest request = HttpRequest.newBuilder().uri(new URI(url)).header("Content-Type", "application/x-www-form-urlencoded").header("Cookie", cookie).header("Referer", "https://weidian.com/").header("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/108.0.0.0 Safari/537.36").POST(BodyPublishers.ofString(requestBody)).build();
            HttpResponse<String> response = httpClient.send(request, BodyHandlers.ofString());
            HttpHeaders headers = new HttpHeaders();
            headers.add("Content-Type", "application/json");
            return new ResponseEntity((String)response.body(), headers, HttpStatus.valueOf(response.statusCode()));
        } catch (Exception e) {
            e.printStackTrace();
            return new ResponseEntity("Get cart list failed: " + e.getMessage(), HttpStatus.INTERNAL_SERVER_ERROR);
        }
    }

    @PostMapping({"/getUserInfo"})
    public ResponseEntity<String> getUserInfo(@RequestParam("cookie") String cookie) {
        try {
            String baseUrl = "https://thor.weidian.com/udccore/udc.user.getUserInfoById/1.0?param=";
            String queryParam = "{}";
            String encodedQueryParam = URLEncoder.encode(queryParam, StandardCharsets.UTF_8);
            String fullUrl = baseUrl + encodedQueryParam;
            HttpClient httpClient = HttpClient.newBuilder().followRedirects(Redirect.NORMAL).build();
            HttpRequest request = HttpRequest.newBuilder().uri(new URI(fullUrl)).header("Content-Type", "application/x-www-form-urlencoded;charset=UTF-8").header("Cookie", cookie).header("Referer", "https://weidian.com/").header("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/108.0.0.0 Safari/537.36").GET().build();
            HttpResponse<String> response = httpClient.send(request, BodyHandlers.ofString());
            HttpHeaders headers = new HttpHeaders();
            headers.add("Content-Type", "application/json");
            return new ResponseEntity((String)response.body(), headers, HttpStatus.valueOf(response.statusCode()));
        } catch (Exception e) {
            e.printStackTrace();
            return new ResponseEntity("Get user info failed: " + e.getMessage(), HttpStatus.INTERNAL_SERVER_ERROR);
        }
    }

    @PostMapping({"/getAddOrderData"})
    public ResponseEntity<String> getAddOrderData(@RequestParam("link") String link, @RequestParam("cookie") String cookie) {
        try {
            HttpClient httpClient = HttpClient.newBuilder().followRedirects(Redirect.NORMAL).build();
            HttpRequest request = HttpRequest.newBuilder().uri(new URI(link)).header("Content-Type", "application/x-www-form-urlencoded;charset=UTF-8").header("Cookie", cookie).header("Referer", "https://weidian.com/").header("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/108.0.0.0 Safari/537.36").GET().build();
            HttpResponse<String> response = httpClient.send(request, BodyHandlers.ofString());
            JsonNode responseBody = NetworkUtil.getDataObject((String)response.body());
            HttpHeaders headers = new HttpHeaders();
            headers.add("Content-Type", "application/json");
            return responseBody != null ? new ResponseEntity(responseBody.toString(), headers, HttpStatus.OK) : new ResponseEntity("Failed to process order data", headers, HttpStatus.BAD_REQUEST);
        } catch (Exception e) {
            e.printStackTrace();
            return new ResponseEntity("Get order data failed: " + e.getMessage(), HttpStatus.INTERNAL_SERVER_ERROR);
        }
    }

    @PostMapping({"/proxy"})
    public ResponseEntity<String> proxyRequest(@RequestParam("url") String url, @RequestParam("cookie") String cookie, @RequestParam(value = "method",defaultValue = "GET") String method, @RequestParam(value = "body",required = false) String body) {
        try {
            HttpClient httpClient = HttpClient.newBuilder().followRedirects(Redirect.NORMAL).build();
            HttpRequest.Builder requestBuilder = HttpRequest.newBuilder().uri(new URI(url)).header("Content-Type", "application/x-www-form-urlencoded;charset=UTF-8").header("Cookie", cookie).header("Referer", "https://weidian.com/").header("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/108.0.0.0 Safari/537.36");
            if ("POST".equalsIgnoreCase(method)) {
                requestBuilder.POST(BodyPublishers.ofString(body != null ? body : ""));
            } else {
                requestBuilder.GET();
            }

            HttpResponse<String> response = httpClient.send(requestBuilder.build(), BodyHandlers.ofString());
            HttpHeaders headers = new HttpHeaders();
            headers.add("Content-Type", "application/json");
            return new ResponseEntity((String)response.body(), headers, HttpStatus.valueOf(response.statusCode()));
        } catch (Exception e) {
            e.printStackTrace();
            return new ResponseEntity("Request failed: " + e.getMessage(), HttpStatus.INTERNAL_SERVER_ERROR);
        }
    }
}
