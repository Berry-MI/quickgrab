// 身份证号码验证
function validateIdNumber(idNumber) {
    // 身份证号码正则表达式
    const idNumberPattern = /(^\d{15}$)|(^\d{18}$)|(^\d{17}(\d|X|x)$)/;
    if (!idNumberPattern.test(idNumber)) {
        return false;
    }

    // 验证18位身份证的最后一位校验码
    if (idNumber.length === 18) {
        const factor = [7, 9, 10, 5, 8, 4, 2, 1, 6, 3, 7, 9, 10, 5, 8, 4, 2];
        const parity = ['1', '0', 'X', '9', '8', '7', '6', '5', '4', '3', '2'];
        let sum = 0;
        let ai = 0;
        let wi = 0;
        for (let i = 0; i < 17; i++) {
            ai = idNumber[i];
            wi = factor[i];
            sum += ai * wi;
        }
        const last = parity[sum % 11];
        if (last.toLowerCase() !== idNumber[17].toLowerCase()) {
            return false;
        }
    }
    return true;
}

// JSON格式验证
function validateJSON(str) {
    if (!str) return false;
    try {
        const obj = JSON.parse(str);
        return typeof obj === 'object' && obj !== null;
    } catch (e) {
        return false;
    }
}

// 格式化JSON字符串
function formatJSON(str) {
    try {
        const obj = JSON.parse(str);
        return JSON.stringify(obj, null, 2);
    } catch (e) {
        return str;
    }
}

export {validateIdNumber, validateJSON, formatJSON};