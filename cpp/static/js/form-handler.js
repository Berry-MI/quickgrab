import {setItemInfo} from "./api-handler.js";
import {setButtonLoading, showAlert, throttle} from "./util.js";
import {formatJSON, validateIdNumber, validateJSON} from "./form-validation.js";

document.addEventListener('DOMContentLoaded', function () {
    toggleFields();
    toggleAdvancedFields();
    toggleEmailField();
    toggleShippingFeeField();
    setupValidation();
    setupAutoDelay();
});

function setupValidation() {
    // 下单模板验证
    const orderTemplateInput = document.getElementById('orderTemplate');
    orderTemplateInput.addEventListener('input', function () {
        const value = this.value.trim();
        if (value) {
            if (!validateJSON(value)) {
                this.classList.add('invalid');
                showAlert('请输入有效的JSON格式');
            } else {
                this.classList.remove('invalid');
                // 格式化JSON
                this.value = formatJSON(value);
            }
        } else {
            this.classList.remove('invalid');
        }
    });

    // 身份证验证
    const idNumberInput = document.getElementById('idNumber');
    idNumberInput.addEventListener('input', function () {
        const value = this.value.trim();
        if (value) {
            if (!validateIdNumber(value)) {
                this.classList.add('invalid');
                showAlert('请输入有效的身份证号码');
            } else {
                this.classList.remove('invalid');
            }
        } else {
            this.classList.remove('invalid');
        }
    });
}

document.getElementById('type').addEventListener('change', toggleFields);
document.getElementById('advancedMode').addEventListener('change', toggleAdvancedFields);
document.getElementById('emailReminder').addEventListener('change', toggleEmailField);
document.getElementById('manualShipping').addEventListener('change', toggleShippingFeeField);
document.getElementById('submitRequestButton').addEventListener('click', throttle(handleSubmit, 200));
document.getElementById('cookiesAndLink').addEventListener('input', parseCookiesAndLink);
document.getElementById('setStartTime').addEventListener('click', setType);
document.getElementById('link').addEventListener('input', setType);
document.getElementById('cookies').addEventListener('input', setType);

// 查询备注按钮点击事件
document.getElementById('fetchNoteButton').addEventListener('click', async function () {
    const button = this;
    try {
        setButtonLoading(button, true, '查询中...');
        // 这里添加查询备注的逻辑
        await new Promise(resolve => setTimeout(resolve, 1000)); // 模拟查询延迟
    } finally {
        setButtonLoading(button, false);
    }
});

// 填写备注按钮点击事件
document.getElementById('setNoteButton').addEventListener('click', async function () {
    const button = this;
    try {
        setButtonLoading(button, true, '处理中...');
        // 这里添加填写备注的逻辑
        await new Promise(resolve => setTimeout(resolve, 1000)); // 模拟处理延迟
    } finally {
        setButtonLoading(button, false);
    }
});

export async function parseCookiesAndLink() {
    let cookiesAndLink = JSON.parse(this.value);
    let link = cookiesAndLink.orderUrl;
    let cookies = cookiesAndLink.cookie;
    document.getElementById('link').value = link;
    document.getElementById('cookies').value = cookies;
    await setType();
}

export async function setType() {
    try {
        const itemLinkCriteria = /(items=).+(source_id=)/;
        const userLinkCriteria = /(userid=)/;
        let link = document.getElementById('link').value;
        let cookies = document.getElementById('cookies').value;

        if (itemLinkCriteria.test(link)) {
            const ret = await setItemInfo();
            if (ret === 1) {
                document.getElementById('type').value = '1';
                showAlert('识别为定时开售，已自动设置时间');
            } else if (ret === 2) {
                document.getElementById('type').value = '3';
                showAlert('识别为捡漏模式');
            } else {
                showAlert('识别失败');
            }
        } else if (userLinkCriteria.test(link)) {
            document.getElementById('type').value = '2';
            showAlert('识别为手动上架');
        } else {
            showAlert('无法识别类型，请手动选择');
        }
        toggleFields();
    } catch (e) {
        console.error('Failed to set type: ', e);
    }
}

function toggleFields() {
    const typeSelect = document.getElementById('type');
    const selectedType = typeSelect.value;
    const link = document.getElementById('link');
    const commodityKeywordField = document.getElementById('commodityKeywordField');
    const quantityField = document.getElementById('quantityField');
    const predictModeContainer = document.querySelector('#orderTemplateField .switch-container');
    const autoPickContainer = document.querySelector('#autoPick').closest('.switch-container');

    if (selectedType === '1') {// 1: 定时开售
        link.placeholder = '请输入订单链接';
        commodityKeywordField.style.display = 'none';
        quantityField.style.display = 'none';
        predictModeContainer.style.display = 'none';
        autoPickContainer.style.display = 'flex'; // 显示自动捡漏开关
    } else if (selectedType === '2') {// 2: 手动上架
        link.placeholder = '请输入店铺链接';
        commodityKeywordField.style.display = 'flex';
        quantityField.style.display = 'flex';
        predictModeContainer.style.display = 'flex';
        autoPickContainer.style.display = 'flex'; // 显示自动捡漏开关
    } else if (selectedType === '3') {// 3: 捡漏模式
        link.placeholder = '请输入订单链接';
        commodityKeywordField.style.display = 'none';
        quantityField.style.display = 'flex';
        predictModeContainer.style.display = 'none';
        autoPickContainer.style.display = 'none'; // 隐藏自动捡漏开关
    }
}

function toggleAdvancedFields() {
    const advancedMode = document.getElementById('advancedMode').checked;
    const frequencyField = document.getElementById('frequencyField');
    const messageField = document.getElementById('messageField');
    const idNumberField = document.getElementById('idNumberField');

    frequencyField.style.display = advancedMode ? 'flex' : 'none';
    messageField.style.display = advancedMode ? 'flex' : 'none';
    idNumberField.style.display = advancedMode ? 'flex' : 'none';
}

function toggleEmailField() {
    const emailReminder = document.getElementById('emailReminder').checked;
    const emailField = document.getElementById('emailField');

    emailField.style.display = emailReminder ? 'flex' : 'none';
}

function toggleShippingFeeField() {
    const manualShipping = document.getElementById('manualShipping').checked;
    const shippingFeeField = document.getElementById('shippingFeeField');

    shippingFeeField.style.display = manualShipping ? 'flex' : 'none';
}

export function getInputValue() {
    const type = document.getElementById('type').value;
    const buyerId = document.getElementById('buyerSelect').value;
    const startTime = document.getElementById('startTime').value;
    const endTime = document.getElementById('endTime').value;
    const delay = document.getElementById('delay').value;
    const frequency = document.getElementById('frequency').value;
    const actualEarnings = document.getElementById('actualEarnings').value;
    const message = document.getElementById('message').value;
    const emailReminder = document.getElementById('emailReminder').checked;
    const email = document.getElementById('email').value;
    const steadyOrder = document.getElementById('steadyOrder').checked;
    const hasExpireDate = document.getElementById('hasExpireDate').checked;
    const autoPick = document.getElementById('autoPick').checked;
    const manualShipping = document.getElementById('manualShipping').checked;
    const shippingFee = document.getElementById('shippingFee').value;
    const quickMode = document.getElementById('quickMode').checked;
    const useIpProxy = document.getElementById('useIpProxy').checked;
    let link, cookies, orderTemplate, idNumber, keyword, quantity = 1, extension;

    link = document.getElementById('link').value;
    cookies = document.getElementById('cookies').value;

    if (type === '1') {
        orderTemplate = document.getElementById('orderTemplate').value;
        idNumber = document.getElementById('idNumber').value;
    } else if (type === '2') {
        orderTemplate = document.getElementById('orderTemplate').value;
        keyword = document.getElementById('commodityKeyword').value;
        quantity = document.getElementById('quantity').value;
    } else if (type === '3') {
        orderTemplate = document.getElementById('orderTemplate').value;
        idNumber = document.getElementById('idNumber').value;
        quantity = document.getElementById('quantity').value;
    }

    if (orderTemplate == '') {
        orderTemplate = null;
    }

    // 保存扩展信息
    extension = {
        emailReminder,
        email,
        steadyOrder,
        hasExpireDate,
        autoPick,
        manualShipping,
        shippingFee: parseFloat(shippingFee),
        quickMode,
        useProxy: useIpProxy
    };

    extension = JSON.stringify(extension);

    // 创建用户信息对象
    const userInfo = {
        orderNote: document.getElementById('orderNote').value.trim(),
        orderChannel: document.getElementById('orderChannel').value
    };

    const requestData = {
        buyerId,
        link,
        cookies,
        orderTemplate,
        message,
        idNumber,
        keyword,
        startTime,
        endTime,
        quantity,
        delay,
        frequency,
        type,
        status: 1, // 默认状态为等待
        actualEarnings,
        extension,
        userInfo: JSON.stringify(userInfo)
    };
    return requestData;
}

async function handleSubmit() {
    const orderTemplate = document.getElementById('orderTemplate').value.trim();
    const idNumber = document.getElementById('idNumber').value.trim();

    // 如果填写了下单模板，验证JSON格式
    if (orderTemplate && !validateJSON(orderTemplate)) {
        showAlert('下单模板必须是有效的JSON格式');
        return;
    }

    // 如果填写了身份证，验证格式
    if (idNumber && !validateIdNumber(idNumber)) {
        showAlert('请输入有效的身份证号码');
        return;
    }

    const requestData = getInputValue();
    if (!validateFields(requestData)) {
        return;
    }
    sendRequest(requestData);
}

function validateFields(data) {
    const itemLinkCriteria = /(items=).+(source_id=)/;
    const shopLinkCriteria = /(userid=)/;
    const cookiesCriteria = /(login_token=)/;
    let info = '';

    if (data.type == '1' || data.type == '3') {
        if (!itemLinkCriteria.test(data.link)) {
            info += "订单链接不正确或缺少必要的参数\n";
        }
    } else if (data.type == '2') {
        if (!shopLinkCriteria.test(data.link)) {
            info += "店铺链接不正确或缺少必要的参数\n";
        }
    }
    if (!cookiesCriteria.test(data.cookies)) {
        info += "Cookie格式不正确或缺少login_token字段\n";
    }

    if (data.startTime == '') {
        info += '请输入开售时间\n';
    }
    if (data.endTime == '') {
        info += '请输入结束时间\n';
    }
    if (data.delay == '') {
        info += '请输入延迟时间\n';
    }
    if (data.frequency == '') {
        info += '请输入刷新频率\n';
    }
    if (data.actualEarnings == '') {
        info += '请输入实际收益\n';
    }
    if (data.quantity == '') {
        info += '请输入数量\n';
    }

    if (info != '') {
        showAlert(info);
        return false;
    }
    return true;
}

// 添加自动延时开关的处理函数
function setupAutoDelay() {
    const autoDelaySwitch = document.getElementById('autoDelay');
    const delayInput = document.getElementById('delay');
    let lastDelay = delayInput.value; // 保存上一次的延迟值

    autoDelaySwitch.addEventListener('change', function () {
        if (!this.checked) {
            lastDelay = delayInput.value; // 保存当前值
            delayInput.value = 0; // 设置为0
        } else {
            delayInput.value = lastDelay; // 恢复上一次的值
        }
    });
}
