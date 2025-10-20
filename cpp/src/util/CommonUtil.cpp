#include "quickgrab/util/CommonUtil.hpp"

#include "quickgrab/util/HttpClient.hpp"
#include "quickgrab/util/JsonUtil.hpp"
#include "quickgrab/util/Logging.hpp"

#include <boost/json.hpp>

#include <algorithm>
#include <boost/beast/http/status.hpp>

#include <chrono>
#include <cmath>
#include <cctype>
#include <iomanip>
#include <optional>
#include <stdexcept>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <unordered_map>
#include <vector>

namespace quickgrab::util {
namespace {

std::string_view trimView(std::string_view view) {
    while (!view.empty() && std::isspace(static_cast<unsigned char>(view.front()))) {
        view.remove_prefix(1);
    }
    while (!view.empty() && std::isspace(static_cast<unsigned char>(view.back()))) {
        view.remove_suffix(1);
    }
    return view;
}

std::optional<int> parseIntLike(std::string_view text) {
    text = trimView(text);
    if (text.empty()) {
        return std::nullopt;
    }

    std::string normalized(text);
    std::string lowered = normalized;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (lowered == "true" || lowered == "yes") {
        return 1;
    }
    if (lowered == "false" || lowered == "no") {
        return 0;
    }

    try {
        size_t consumed = 0;
        int value = std::stoi(normalized, &consumed, 10);
        if (consumed == normalized.size()) {
            return value;
        }
    } catch (const std::exception&) {
    }

    try {
        size_t consumed = 0;
        double value = std::stod(normalized, &consumed);
        if (consumed == normalized.size()) {
            return static_cast<int>(std::lround(value));
        }
    } catch (const std::exception&) {
    }

    return std::nullopt;
}

std::optional<int> parseIntLike(const boost::json::value& value) {
    if (value.is_int64()) {
        return static_cast<int>(value.as_int64());
    }
    if (value.is_uint64()) {
        return static_cast<int>(value.as_uint64());
    }
    if (value.is_double()) {
        return static_cast<int>(std::lround(value.as_double()));
    }
    if (value.is_bool()) {
        return value.as_bool() ? 1 : 0;
    }
    if (value.is_string()) {
        const auto& str = value.as_string();
        return parseIntLike(std::string_view{str.c_str(), str.size()});
    }
    return std::nullopt;
}

bool parseFlag(const boost::json::value& value, bool fallback = false) {
    if (auto parsed = parseIntLike(value)) {
        return *parsed != 0;
    }
    return fallback;
}

bool parseFlag(std::string_view text, bool fallback = false) {
    if (auto parsed = parseIntLike(text)) {
        return *parsed != 0;
    }
    return fallback;
}

const boost::json::value* findValueNodeByKey(const boost::json::value& value, std::string_view key) {
    if (value.is_object()) {
        const auto& obj = value.as_object();
        if (auto it = obj.if_contains(key.data())) {
            return it;
        }
        for (const auto& entry : obj) {
            if (auto found = findValueNodeByKey(entry.value(), key)) {
                return found;
            }
        }
    } else if (value.is_array()) {
        for (const auto& element : value.as_array()) {
            if (auto found = findValueNodeByKey(element, key)) {
                return found;
            }
        }
    }
    return nullptr;
}

std::string toString(const boost::json::value& value) {
    if (value.is_string()) {
        const auto& str = value.as_string();
        return std::string(str.c_str(), str.size());
    }
    if (value.is_int64()) {
        return std::to_string(value.as_int64());
    }
    if (value.is_uint64()) {
        return std::to_string(value.as_uint64());
    }
    if (value.is_double()) {
        std::ostringstream oss;
        oss.setf(std::ios::fixed);
        oss << std::setprecision(2) << value.as_double();
        return oss.str();
    }
    if (value.is_bool()) {
        return value.as_bool() ? "true" : "false";
    }
    return {};
}

int toInt(const boost::json::value& value, int fallback = 0) {
    if (value.is_int64()) {
        return static_cast<int>(value.as_int64());
    }
    if (value.is_uint64()) {
        return static_cast<int>(value.as_uint64());
    }
    if (value.is_double()) {
        return static_cast<int>(std::lround(value.as_double()));
    }
    if (value.is_bool()) {
        return value.as_bool() ? 1 : 0;
    }
    if (value.is_string()) {
        try {
            return std::stoi(std::string(value.as_string().c_str()));
        } catch (const std::exception&) {
        }
    }
    return fallback;
}

double toDouble(const boost::json::value& value, double fallback = 0.0) {
    if (value.is_double()) {
        return value.as_double();
    }
    if (value.is_int64()) {
        return static_cast<double>(value.as_int64());
    }
    if (value.is_uint64()) {
        return static_cast<double>(value.as_uint64());
    }
    if (value.is_bool()) {
        return value.as_bool() ? 1.0 : 0.0;
    }
    if (value.is_string()) {
        try {
            return std::stod(std::string(value.as_string().c_str()));
        } catch (const std::exception&) {
        }
    }
    return fallback;
}

std::string formatPrice(double value) {
    std::ostringstream oss;
    oss.setf(std::ios::fixed);
    oss << std::setprecision(2) << value;
    return oss.str();
}

double normalizeUnitPrice(double value) {
    return std::round(value * 100.0) / 100.0;
}

const boost::json::object* asObjectPtr(const boost::json::value* value) {
    if (value && value->is_object()) {
        return &value->as_object();
    }
    return nullptr;
}

const boost::json::array* asArrayPtr(const boost::json::value* value) {
    if (value && value->is_array()) {
        return &value->as_array();
    }
    return nullptr;
}

std::string decodeHtmlEntities(std::string_view input) {
    std::string output;
    output.reserve(input.size());

    for (std::size_t i = 0; i < input.size();) {
        if (input[i] == '&') {
            auto semicolon = input.find(';', i + 1);
            if (semicolon != std::string::npos) {
                std::string_view entity = input.substr(i + 1, semicolon - i - 1);
                bool consumed = true;
                if (entity == "quot") {
                    output.push_back('"');
                } else if (entity == "amp") {
                    output.push_back('&');
                } else if (entity == "lt") {
                    output.push_back('<');
                } else if (entity == "gt") {
                    output.push_back('>');
                } else if (entity == "apos") {
                    output.push_back(static_cast<char>(39));
                } else if (!entity.empty() && entity.front() == '#') {
                    int base = 10;
                    std::string_view digits = entity.substr(1);
                    if (!digits.empty() && (digits.front() == 'x' || digits.front() == 'X')) {
                        base = 16;
                        digits.remove_prefix(1);
                    }
                    try {
                        int codePoint = std::stoi(std::string(digits), nullptr, base);
                        output.push_back(static_cast<char>(codePoint));
                    } catch (const std::exception&) {
                        consumed = false;
                    }
                } else {
                    consumed = false;
                }

                if (consumed) {
                    i = semicolon + 1;
                    continue;
                }
            }
        }

        output.push_back(input[i]);
        ++i;
    }

    return output;
}

std::optional<std::string> extractDataObjAttribute(const std::string& html) {
    constexpr std::string_view kMarker = "__rocker-render-inject__";
    auto idPos = html.find(kMarker);
    if (idPos == std::string::npos) {
        return std::nullopt;
    }

    auto dataPos = html.find("data-obj=", idPos);
    if (dataPos == std::string::npos) {
        return std::nullopt;
    }
    dataPos += std::string_view("data-obj=").size();
    if (dataPos >= html.size()) {
        return std::nullopt;
    }

    char quote = html[dataPos];
    if (quote != '"' && quote != '\'') {
        return std::nullopt;
    }
    ++dataPos;

    std::string encoded;
    while (dataPos < html.size() && html[dataPos] != quote) {
        encoded.push_back(html[dataPos]);
        ++dataPos;
    }

    if (dataPos >= html.size()) {
        return std::nullopt;
    }

    return decodeHtmlEntities(encoded);
}

std::optional<boost::json::object> fetchItemModel(HttpClient& httpClient, const std::string& itemId) {
    std::vector<HttpClient::Header> headers{{"Content-Type", "application/x-www-form-urlencoded;charset=UTF-8"},
                                            {"Referer", "https://weidian.com/"},
                                            {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/108.0.0.0 Safari/537.36"}};

    const std::string url = "https://weidian.com/item.html?itemID=" + itemId;

    try {
        auto response = httpClient.fetch("GET",
                                         url,
                                         headers,
                                         "",
                                         itemId,
                                         std::chrono::seconds{15},
                                         true,
                                         5,
                                         nullptr,
                                         false);

        if (response.result() != boost::beast::http::status::ok) {
            util::log(util::LogLevel::warn,
                      "获取商品页面失败 itemId=" + itemId + " 状态码=" + std::to_string(static_cast<int>(response.result())));
            return std::nullopt;
        }

        auto payload = extractDataObjAttribute(response.body());
        if (!payload) {
            util::log(util::LogLevel::warn, "未找到商品页面的 data-obj 属性 itemId=" + itemId);
            return std::nullopt;
        }

        boost::json::value json;
        try {
            json = quickgrab::util::parseJson(*payload);
        } catch (const std::exception& ex) {
            util::log(util::LogLevel::warn,
                      "解析商品页面数据失败 itemId=" + itemId + " 错误=" + ex.what());
            return std::nullopt;
        }

        if (!json.is_object()) {
            return std::nullopt;
        }

        const auto& root = json.as_object();
        const auto* status = asObjectPtr(root.if_contains("status"));
        if (!status) {
            return std::nullopt;
        }

        auto messageIt = status->if_contains("message");
        if (!messageIt || !messageIt->is_string()) {
            return std::nullopt;
        }

        if (messageIt->as_string() != "OK") {
            return std::nullopt;
        }

        const auto* result = asObjectPtr(root.if_contains("result"));
        if (!result) {
            return std::nullopt;
        }

        if (const auto* model = asObjectPtr(result->if_contains("default_model"))) {
            return *model;
        }
    } catch (const std::exception& ex) {
        util::log(util::LogLevel::warn,
                  "获取商品页面数据异常 itemId=" + itemId + " 错误=" + ex.what());
    }

    return std::nullopt;
}

boost::json::object buildExpireDateInfo(const std::string& expireDate) {
    std::string cleaned = expireDate;
    if (auto pos = cleaned.find(" 有效"); pos != std::string::npos) {
        cleaned.erase(pos, std::string::npos);
    }

    auto delimiter = cleaned.find(" 至 ");
    boost::json::object validDateInfo;

    auto trimCopy = [](const std::string& text) {
        std::string_view view{text};
        view = trimView(view);
        return std::string(view);
    };

    if (delimiter != std::string::npos) {
        std::string start = cleaned.substr(0, delimiter);
        std::string end = cleaned.substr(delimiter + std::string(" 至 ").size());
        validDateInfo["validStartDate"] = trimCopy(start);
        validDateInfo["validEndDate"] = trimCopy(end);
        validDateInfo["validType"] = 4;
    } else {
        validDateInfo["validType"] = 1;
    }

    boost::json::object conveyInfo;
    conveyInfo["valid_date_info"] = std::move(validDateInfo);
    return conveyInfo;
}

std::optional<std::string> findValueByKey(const boost::json::value& value, std::string_view key) {
    if (value.is_object()) {
        const auto& obj = value.as_object();
        if (auto it = obj.if_contains(key.data())) {
            auto text = toString(*it);
            if (!text.empty()) {
                return text;
            }
        }
        for (const auto& entry : obj) {
            if (auto result = findValueByKey(entry.value(), key)) {
                return result;
            }
        }
    } else if (value.is_array()) {
        for (const auto& element : value.as_array()) {
            if (auto result = findValueByKey(element, key)) {
                return result;
            }
        }
    }
    return std::nullopt;
}

std::unordered_map<std::string, std::string> buildCalendarMap(const boost::json::object& dataObj) {
    std::unordered_map<std::string, std::string> calendar;
    if (auto confirm = dataObj.if_contains("confirmOrderParam")) {
        if (const auto* confirmObj = asObjectPtr(confirm)) {
            if (const auto* list = asArrayPtr(confirmObj->if_contains("item_list"))) {
                for (const auto& item : *list) {
                    if (!item.is_object()) {
                        continue;
                    }
                    const auto& itemObj = item.as_object();
                    auto idIt = itemObj.if_contains("item_id");
                    auto dateIt = itemObj.if_contains("calendar_date");
                    if (idIt && idIt->is_string() && dateIt && dateIt->is_string()) {
                        std::string itemId(idIt->as_string().c_str());
                        std::string calendarDate(dateIt->as_string().c_str());
                        if (!itemId.empty() && !calendarDate.empty()) {
                            calendar[itemId] = calendarDate;
                        }
                    }
                }
            }
        }
    }
    return calendar;
}

boost::json::object parseObjectValue(const boost::json::value& value) {
    if (value.is_object()) {
        return value.as_object();
    }
    if (value.is_string()) {
        try {
            auto parsed = quickgrab::util::parseJson(std::string(value.as_string().c_str()));
            if (parsed.is_object()) {
                return parsed.as_object();
            }
        } catch (const std::exception&) {
        }
    }
    return {};
}

double parseExpressFeeFromDesc(const std::string& desc,
    double shopPrice,
    double currentFee) {
    bool freeShipping = false;
    std::regex freePattern("满(\\d+(?:\\.\\d+)?)元包邮");
    std::smatch match;

    if (std::regex_search(desc, match, freePattern)) {
        try {
            double threshold = std::stod(match[1].str());
            if (shopPrice >= threshold) {
                freeShipping = true;
                currentFee = 0.0;
            }
        }
        catch (const std::exception&) {
            // 忽略解析失败
        }
    }

    if (!freeShipping) {
        std::regex feePattern("(\\d+(?:\\.\\d+)?)元起?");
        if (std::regex_search(desc, match, feePattern)) {   
            try {
                currentFee = std::stod(match[1].str());
            }
            catch (const std::exception&) {
                // 忽略解析失败
            }
        }
    }
    return currentFee;
}


std::string readObjectString(const boost::json::object& obj, const char* key) {
    if (auto it = obj.if_contains(key)) {
        return toString(*it);
    }
    return {};
}

int readObjectInt(const boost::json::object& obj, const char* key, int fallback = 0) {
    if (auto it = obj.if_contains(key)) {
        return toInt(*it, fallback);
    }
    return fallback;
}

double readObjectDouble(const boost::json::object& obj, const char* key, double fallback = 0.0) {
    if (auto it = obj.if_contains(key)) {
        return toDouble(*it, fallback);
    }
    return fallback;
}

} // namespace

std::optional<boost::json::object> generateOrderParameters(const model::Request& request,
                                                           const boost::json::object& dataObj,
                                                           bool includeInvalid,
                                                           HttpClient* httpClient) {
    boost::json::object params;

    std::vector<std::string_view> shopListNames = {"shop_list"};
    if (includeInvalid) {
        shopListNames.emplace_back("invalid_shop_list");
    }

    std::vector<std::string_view> itemListNames = {"item_list"};
    if (includeInvalid) {
        itemListNames.emplace_back("invalid_item_list");
    }

    auto calendarDates = buildCalendarMap(dataObj);
    auto extension = parseObjectValue(request.extension);
    bool extensionHasExpireDate = false;
    if (auto it = extension.if_contains("hasExpireDate")) {
        extensionHasExpireDate = parseFlag(*it);
    }
    std::unordered_map<std::string, boost::json::object> itemModelCache;

    auto getItemModel = [&](const std::string& itemId) -> boost::json::object* {
        if (!httpClient || itemId.empty()) {
            return nullptr;
        }

        if (auto cached = itemModelCache.find(itemId); cached != itemModelCache.end()) {
            return cached->second.empty() ? nullptr : &cached->second;
        }

        if (auto model = fetchItemModel(*httpClient, itemId)) {
            auto [iter, _] = itemModelCache.emplace(itemId, std::move(*model));
            return &iter->second;
        }

        itemModelCache.emplace(itemId, boost::json::object{});
        return nullptr;
    };

    bool manualShipping = false;
    double manualShippingFee = 0.0;
    if (auto it = extension.if_contains("manualShipping"); it && it->is_bool() && it->as_bool()) {
        manualShipping = true;
        if (auto fee = extension.if_contains("shippingFee")) {
            manualShippingFee = toDouble(*fee, 0.0);
        }
    }

    boost::json::array shopListArray;
    double totalPayPrice = 0.0;
    int deliveryInfoType = 1;
    bool addedShops = false;

    for (std::string_view listName : shopListNames) {
        bool addedForCurrentList = false;
        const auto* shops = asArrayPtr(dataObj.if_contains(listName.data()));
        if (!shops || shops->empty()) {
            continue;
        }

        for (const auto& shopValue : *shops) {
            if (!shopValue.is_object()) {
                continue;
            }
            const auto& shopObj = shopValue.as_object();

            for (std::string_view itemListName : itemListNames) {
                const auto* items = asArrayPtr(shopObj.if_contains(itemListName.data()));
                if (!items || items->empty()) {
                    continue;
                }

                boost::json::array itemArray;
                double shopOriPrice = 0.0;
                double shopPrice = 0.0;
                std::string firstItemId;

                for (const auto& itemValue : *items) {
                    if (!itemValue.is_object()) {
                        continue;
                    }
                    const auto& itemObj = itemValue.as_object();
                    auto itemId = readObjectString(itemObj, "item_id");
                    if (itemId.empty()) {
                        continue;
                    }

                    if (firstItemId.empty()) {
                        firstItemId = itemId;
                    }

                    int quantity = readObjectInt(itemObj, "quantity", 1);
                    double price = normalizeUnitPrice(readObjectDouble(itemObj, "price"));
                    double oriPrice = normalizeUnitPrice(itemObj.if_contains("ori_price")
                                                            ? readObjectDouble(itemObj, "ori_price", price)
                                                            : price);

                    boost::json::object itemNode;
                    itemNode["item_id"] = itemId;
                    itemNode["quantity"] = quantity;
                    itemNode["item_sku_id"] = readObjectString(itemObj, "item_sku_id");
                    itemNode["price"] = price;
                    itemNode["ori_price"] = oriPrice;

                    if (auto it = calendarDates.find(itemId); it != calendarDates.end()) {
                        itemNode["calendar_date"] = it->second;
                    }

                    bool hasConveyInfo = false;
                    if (auto convey = itemObj.if_contains("item_convey_info"); convey && convey->is_object()) {
                        const auto& conveyObj = convey->as_object();
                        if (auto valid = conveyObj.if_contains("valid_date_info"); valid && valid->is_object() &&
                            !valid->as_object().empty()) {
                            itemNode["item_convey_info"] = conveyObj;
                            hasConveyInfo = true;
                        }
                    }

                    if (!hasConveyInfo && extensionHasExpireDate) {
                        if (auto* model = getItemModel(itemId)) {
                            if (const auto* itemInfo = asObjectPtr(model->if_contains("itemInfo"))) {
                                if (const auto* ticketInfo = asObjectPtr(itemInfo->if_contains("ticketItemInfo"))) {
                                    if (auto expire = ticketInfo->if_contains("expireDate"); expire && expire->is_string()) {
                                        std::string expireText(expire->as_string().c_str());
                                        if (!expireText.empty()) {
                                            itemNode["item_convey_info"] = buildExpireDateInfo(expireText);
                                            hasConveyInfo = true;
                                        }
                                    }
                                }
                            }
                        }
                    }

                    itemArray.emplace_back(std::move(itemNode));
                    shopOriPrice += oriPrice * static_cast<double>(quantity);
                    shopPrice += price * static_cast<double>(quantity);
                }

                if (itemArray.empty()) {
                    continue;
                }

                const boost::json::object* deliveryInfo = nullptr;
                if (auto info = shopObj.if_contains("delivery_info")) {
                    deliveryInfo = asObjectPtr(info);
                }
                if (!deliveryInfo) {
                    if (auto info = dataObj.if_contains("delivery_info")) {
                        deliveryInfo = asObjectPtr(info);
                    }
                }

                if (!deliveryInfo && !firstItemId.empty()) {
                    if (auto* model = getItemModel(firstItemId)) {
                        if (auto info = model->if_contains("delivery_info")) {
                            deliveryInfo = asObjectPtr(info);
                        }
                    }
                }

                double expressFee = 0.0;
                bool expressSet = false;

                if (const auto* expressList = asArrayPtr(shopObj.if_contains("express_list")); expressList && !expressList->empty()) {
                    const auto& first = expressList->front();
                    if (first.is_object()) {
                        expressFee = readObjectDouble(first.as_object(), "express_fee", 0.0);
                        expressSet = true;
                    }
                }

                if (!expressSet && manualShipping) {
                    expressFee = manualShippingFee;
                    expressSet = true;
                    util::log(util::LogLevel::info,
                              "ID=" + std::to_string(request.id) + " 使用手动邮费: " + formatPrice(expressFee));
                }

                if (!expressSet && deliveryInfo) {
                    bool hasExpress = false;
                    bool hasSelfPickup = false;
                    if (const auto* postageInfos = asArrayPtr(deliveryInfo->if_contains("postageInfosNew"));
                        postageInfos && !postageInfos->empty()) {
                        for (const auto& info : *postageInfos) {
                            if (!info.is_object()) {
                                continue;
                            }
                            auto desc = readObjectString(info.as_object(), "deliveryDes");
                            if (desc.find("快递") != std::string::npos) {
                                hasExpress = true;
                            }
                            if (desc.find("自提") != std::string::npos) {
                                hasSelfPickup = true;
                            }
                        }
                    }

                    if (hasExpress && hasSelfPickup) {
                        deliveryInfoType = std::max(deliveryInfoType, 2);
                    } else if (hasSelfPickup) {
                        deliveryInfoType = std::max(deliveryInfoType, 3);
                    }

                    if (auto desc = deliveryInfo->if_contains("expressPostageDesc"); desc && desc->is_string()) {
                        expressFee = parseExpressFeeFromDesc(std::string(desc->as_string().c_str()), shopPrice, expressFee);
                        expressSet = true;
                    }
                }

                double totalShopPrice = shopPrice + expressFee;

                boost::json::object shopNode;
                if (auto shopInfo = asObjectPtr(shopObj.if_contains("shop"))) {
                    auto shopId = readObjectString(*shopInfo, "shop_id");
                    if (!shopId.empty()) {
                        shopNode["shop_id"] = shopId;
                    }
                }
                shopNode["item_list"] = std::move(itemArray);
                shopNode["order_type"] = 3;
                shopNode["ori_price"] = formatPrice(shopOriPrice);
                shopNode["price"] = formatPrice(totalShopPrice);
                shopNode["express_fee"] = formatPrice(expressFee);
                if (!request.message.empty()) {
                    shopNode["note"] = request.message;
                }

                shopListArray.emplace_back(std::move(shopNode));
                totalPayPrice += totalShopPrice;
                addedShops = true;
                addedForCurrentList = true;
                break;
            }
        }

        if (addedForCurrentList) {
            break;
        }
    }

    if (!addedShops) {
        util::log(util::LogLevel::warn, "订单数据中没有可用的商品列表");
        return std::nullopt;
    }

    params["shop_list"] = shopListArray;

    auto assignCustomInfo = [&params](const boost::json::value& value) {
        if (value.is_object()) {
            const auto& obj = value.as_object();
            if (!obj.empty()) {
                params["custom_info"] = obj;
            }
        } else if (value.is_array()) {
            const auto& array = value.as_array();
            if (!array.empty()) {
                params["custom_info"] = array;
            }
        }
    };

    if (!request.orderTemplate.is_null()) {
        assignCustomInfo(request.orderTemplate);
        if (!params.if_contains("custom_info")) {
            if (request.orderTemplate.is_string()) {
                try {
                    auto parsed = quickgrab::util::parseJson(std::string(request.orderTemplate.as_string().c_str()));
                    assignCustomInfo(parsed);
                } catch (const std::exception&) {
                }
            }
        }
    }

    boost::json::object buyerInfo;
    if (const auto* idCardNode = findValueNodeByKey(dataObj, "id_card_flag")) {
        if (parseFlag(*idCardNode) && !request.idNumber.empty()) {
            buyerInfo["buyer_id_no_code"] = request.idNumber;
        }
    }

    auto resolveBuyerAddress = [&](const boost::json::object& root) -> const boost::json::object* {
        if (const auto* direct = asObjectPtr(root.if_contains("buyer_address"))) {
            return direct;
        }
        if (const auto* confirm = asObjectPtr(root.if_contains("confirmOrderParam"))) {
            if (const auto* nested = asObjectPtr(confirm->if_contains("buyer_address"))) {
                return nested;
            }
        }
        if (const auto* result = asObjectPtr(root.if_contains("result"))) {
            if (const auto* nested = asObjectPtr(result->if_contains("buyer_address"))) {
                return nested;
            }
        }
        return nullptr;
    };

    auto copyBuyerContact = [&](const boost::json::object& address) {
        auto assignIfNotEmpty = [&](const char* sourceKey, const char* targetKey) {
            auto value = readObjectString(address, sourceKey);
            if (!value.empty()) {
                buyerInfo[targetKey] = std::move(value);
            }
        };

        auto assignNumeric = [&](const char* sourceKey, const char* targetKey) {
            if (auto it = address.if_contains(sourceKey)) {
                if (it->is_int64()) {
                    buyerInfo[targetKey] = it->as_int64();
                } else if (it->is_uint64()) {
                    buyerInfo[targetKey] = it->as_uint64();
                } else if (it->is_double()) {
                    buyerInfo[targetKey] = it->as_double();
                } else if (it->is_string()) {
                    auto text = std::string(it->as_string().c_str());
                    if (!text.empty()) {
                        buyerInfo[targetKey] = text;
                    }
                }
            }
        };

        assignNumeric("address_id", "buyer_address_id");
        assignNumeric("addr_id", "buyer_address_id");
        assignNumeric("id", "buyer_address_id");

        assignIfNotEmpty("buyer_name", "buyer_name");
        assignIfNotEmpty("name", "buyer_name");

        assignIfNotEmpty("phone", "buyer_telephone");
        assignIfNotEmpty("telephone", "buyer_telephone");

        assignIfNotEmpty("province", "province");
        assignIfNotEmpty("city", "city");
        assignIfNotEmpty("district", "district");
        assignIfNotEmpty("county", "district");
        assignIfNotEmpty("town", "town");
        assignIfNotEmpty("address", "address");
        assignIfNotEmpty("address_detail", "address_detail");
        assignIfNotEmpty("detail", "address_detail");

        if (const auto* region = asObjectPtr(address.if_contains("region"))) {
            auto assignRegion = [&](const char* sourceKey, const char* targetKey) {
                auto value = readObjectString(*region, sourceKey);
                if (!value.empty()) {
                    buyerInfo[targetKey] = std::move(value);
                }
            };

            assignRegion("province", "province");
            assignRegion("city", "city");
            assignRegion("district", "district");
            assignRegion("county", "district");
        }
    };

    if (auto agreementList = asArrayPtr(dataObj.if_contains("agreement_info_list")); agreementList && !agreementList->empty()) {
        boost::json::array agreementTypes;
        for (const auto& agreement : *agreementList) {
            if (!agreement.is_object()) {
                continue;
            }
            const auto& agreementObj = agreement.as_object();
            if (auto typeNode = agreementObj.if_contains("agreement_type")) {
                if (typeNode->is_int64()) {
                    agreementTypes.emplace_back(typeNode->as_int64());
                } else if (typeNode->is_double()) {
                    agreementTypes.emplace_back(typeNode->as_double());
                } else if (typeNode->is_string()) {
                    auto text = std::string(typeNode->as_string().c_str());
                    if (!text.empty()) {
                        try {
                            if (text.find('.') != std::string::npos) {
                                agreementTypes.emplace_back(std::stod(text));
                            } else {
                                agreementTypes.emplace_back(std::stoll(text));
                            }
                        } catch (const std::exception&) {
                            agreementTypes.emplace_back(std::move(text));
                        }
                    }
                }
            }
        }
        if (!agreementTypes.empty()) {
            buyerInfo["agreement_type_list"] = std::move(agreementTypes);
        }
    }

    std::optional<int> deliverTypeHint;
    if (auto it = dataObj.if_contains("only_self_delivery")) {
        deliverTypeHint = parseIntLike(*it);
    }

    int deliverTypeValue = 0;
    int isNoShipAddr = 0;
    if (const auto* noShipNode = findValueNodeByKey(dataObj, "is_no_ship_addr")) {
        isNoShipAddr = parseFlag(*noShipNode) ? 1 : 0;
    } else if (auto noShip = findValueByKey(dataObj, "is_no_ship_addr")) {
        if (parseFlag(*noShip)) {
            isNoShipAddr = 1;
        }
    }
    bool requiresShippingAddress = (isNoShipAddr == 0);

    if (deliverTypeHint && *deliverTypeHint == 1) {
        deliverTypeValue = 1;
        if (const auto* buyerAddress = asObjectPtr(dataObj.if_contains("buyer_address"))) {
            if (const auto* selfAddresses = asArrayPtr(buyerAddress->if_contains("self_delivery_address"));
                selfAddresses && !selfAddresses->empty()) {
                const auto& firstAddr = selfAddresses->front();
                if (firstAddr.is_object()) {
                    buyerInfo["self_address_id"] = readObjectInt(firstAddr.as_object(), "address_id");
                }
            }
            auto name = readObjectString(*buyerAddress, "buyer_name");
            auto phone = readObjectString(*buyerAddress, "phone");
            if (!name.empty()) {
                buyerInfo["buyer_name"] = name;
            }
            if (!phone.empty()) {
                buyerInfo["buyer_telephone"] = phone;
            }
        }
    } else if (deliverTypeHint && *deliverTypeHint == 0) {
        if (const auto* expressTypes = asObjectPtr(dataObj.if_contains("express_types"))) {
            if (const auto* typeList = asArrayPtr(expressTypes->if_contains("type_list")); typeList) {
                for (const auto& typeValue : *typeList) {
                    if (typeValue.is_object()) {
                        auto desc = readObjectString(typeValue.as_object(), "desc");
                        if (desc == "快递") {
                            deliverTypeValue = 0;
                            break;
                        }
                    }
                }
            }
        }
    } else if (deliveryInfoType == 3) {
        deliverTypeValue = 1;
    }

    if (!deliverTypeHint && deliveryInfoType == 3) {
        deliverTypeValue = 1;
    }

    params["deliver_type"] = deliverTypeValue;

    params["is_no_ship_addr"] = isNoShipAddr;

    if (requiresShippingAddress && deliverTypeValue == 0) {
        if (const auto* buyerAddress = resolveBuyerAddress(dataObj)) {
            copyBuyerContact(*buyerAddress);
        } else {
            util::log(util::LogLevel::warn,
                      "ID=" + std::to_string(request.id) + " 缺少快递收货地址信息，模板要求提供收货地址");
        }
    }

    params["total_pay_price"] = formatPrice(totalPayPrice);
    params["channel"] = "maijiaban";
    if (auto sourceId = readObjectString(dataObj, "source_id"); !sourceId.empty()) {
        params["source_id"] = sourceId;
    }

    params["buyer"] = std::move(buyerInfo);

    return params;
}

} // namespace quickgrab::util

