// 用于处理与服务器的交互
import { copyToClipboard } from './clipboard-handler.js';
import {showAlert, setButtonLoading} from './util.js';

window.sendRequest = sendRequest;
window.checkLatency = checkLatency;

async function sendRequest(data) {
    const submitButton = document.getElementById('submitRequestButton');
    try {
        setButtonLoading(submitButton, true, '提交中...');

        const response = await fetch('api/submitRequest', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(data)
        });

        // 检查响应状态是否为2xx
        if (!response.ok) {
            throw new Error('网络响应不正常: ' + response.statusText);
        }

        const responseData = await response.json();

        // 检查响应数据是否为有效的JSON
        if (!responseData) {
            throw new Error('响应数据不是有效的JSON');
        }

        handleResponse(responseData);
    } catch (error) {
        console.error('获取操作中出现问题:', error);
        showAlert('请求失败，请稍后重试');
    } finally {
        setButtonLoading(submitButton, false);
    }
}

// 保存回执内容
let lastReceipt = '';

function handleResponse(data) {
    // 处理服务器返回的信息
    const status = data.status;
    const result = data.result;
    const responseModal = document.getElementById('responseModal');
    const modalOverlay = document.getElementById('modalOverlay');
    const responseIcon = document.querySelector('.response-icon');
    const responseStatus = document.querySelector('.response-status');
    const responseDetails = document.querySelector('.response-details');
    const modalFooter = document.querySelector('#responseModal .modal-footer');

    if (status.code !== 200) {
        // 显示错误信息
        responseIcon.className = 'response-icon error';
        responseStatus.textContent = '请求失败';
        responseDetails.textContent = status.description;
        // 错误时只显示确认按钮
        modalFooter.innerHTML = `
            <button class="modal-button primary" onclick="closeResponseModal()">
                <i class="ri-check-line"></i>
                确认
            </button>
        `;
    } else {
        // 显示成功信息
        const request = result.request;
        lastReceipt = generateReceipt(request);
        copyToClipboard(lastReceipt);

        responseIcon.className = 'response-icon success';
        responseStatus.textContent = '请求成功';
        responseDetails.textContent = `用户信息：${request.userInfo.nickName} (${request.userInfo.telephone})
请求编号：${request.id}
开始时间：${new Date(request.startTime).toLocaleString()}
网络延迟：${result.networkDelay}ms`;

        // 成功时显示确认和复制回执按钮
        modalFooter.innerHTML = `
            <button class="modal-button secondary" onclick="copyLastReceipt()">
                <i class="ri-file-copy-line"></i>
                复制回执
            </button>
            <button class="modal-button primary" onclick="closeResponseModal()">
                <i class="ri-check-line"></i>
                确认
            </button>
        `;
    }

    // 显示模态框和遮罩层
    modalOverlay.style.display = 'block';
    responseModal.style.display = 'block';
}

// 复制最后一次的回执
window.copyLastReceipt = function () {
    if (lastReceipt) {
        copyToClipboard(lastReceipt);
        showAlert('回执已复制', 'success');
    }
}

// 关闭返回信息模态框
window.closeResponseModal = function () {
    const responseModal = document.getElementById('responseModal');
    const modalOverlay = document.getElementById('modalOverlay');
    responseModal.style.display = 'none';
    modalOverlay.style.display = 'none';
}

function generateReceipt(request) {
    const telephone = request.userInfo.telephone;
    const nickName = request.userInfo.nickName;
    let startTime = request.startTime;
    startTime = new Date(startTime).toLocaleString();
    const type = request.type;
    const delay = request.delay;
    const orderTemplate = request.orderTemplate;

    const delayInSeconds = Math.round(delay / 1000);

    return `时间：${startTime}
账号：${telephone} (${nickName})
方式：${getTypeText(type)}
延迟：${delayInSeconds} 秒
备注：${orderTemplate ? '有' : '无'}
`;
}

function getTypeText(type) {
    switch (type) {
        case 1:
            return '定时抢购';
        case 2:
            return '手动上架';
        case 3:
            return '捡漏模式';
        default:
            return '未知';
    }
}

// 函数用于发送一个请求到服务器并计算延迟

function checkLatency() {
    const data = getInputValue();
    if (data == null) {
        return;
    }
    fetch('/api/checkLatency', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify(data),
    })
        .then(response => response.text())
        .then(data => {
            if (data != '') {
                document.getElementById('latency').innerText = `网络延迟: ${data}ms`; // 显示延迟
            } else {
                showAlert('没有获取到', 'info');
            }
        })
        .catch(error => showAlert('查询失败:' + error, 'error'));
}

export async function setItemInfo() {
    const setTimeButton = document.getElementById('setStartTime');
    const link = document.getElementById('link').value;
    const cookies = document.getElementById('cookies').value;
    if (link === '') {
        showAlert('请先填写订单链接');
        return 0;
    }
    if(cookies === '') {
        showAlert('请先填写cookie');
        return 0;
    }

    try {
        setButtonLoading(setTimeButton, true, '');

        const response = await fetch('/api/fetchItemInfo', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/x-www-form-urlencoded',
            },
            body: 'link=' + encodeURIComponent(link)+'&cookies=' + encodeURIComponent(cookies),
        });

        const data = await response.json();
        return processResponseData(data);

    } catch (error) {
        console.error('查询失败:', error);
        showAlert('查询失败: ' + error, 'error');
        return 0;
    } finally {
        setButtonLoading(setTimeButton, false);
    }
}

function processResponseData(data) {
    if (data.status.code === 200) {
        const result = data.result;

        if (result.hasExpireDate) {
            document.getElementById('hasExpireDate').checked = true;
        }

        if (result.isFutureSold) {
            setStartAndEndTime(result.futureSoldTime);
            showAlert('商品未开售，已设置时间和延时', 'info');
            // 如果返回了延迟增量，设置自动延时
            if (result.delayIncrement !== undefined) {
                document.getElementById('autoDelay').checked = true;
                document.getElementById('delay').value = result.delayIncrement;
            }
            // 处理类目数量
            if (result.categoryCount !== undefined && result.categoryCount > 4) {
                document.getElementById('steadyOrder').checked = true;
            }
            return 1; // 未来预售
        } else {
            showAlert('商品已开售', 'info');
            return 2; // 现在售卖
        }

    } else {
        showAlert(data.status.description, 'error');
    }
    return 0;
}

function setStartAndEndTime(futureSoldTime) {
    const date = new Date(futureSoldTime);

    // 将时间转换为UTC+8（北京时间）
    const timeZone = 'Asia/Shanghai';
    const startTime = formatDateTime(date, timeZone);

    // 设置开始时间
    document.getElementById('startTime').value = startTime;

    // 设置结束时间为开始时间的10分钟后
    date.setMinutes(date.getMinutes() + 10);
    const endTime = formatDateTime(date, timeZone);
    document.getElementById('endTime').value = endTime;
}

function formatDateTime(date, timeZone) {
    const localDate = new Date(date.toLocaleString('en-US', { timeZone: timeZone }));
    const year = localDate.getFullYear();
    const month = String(localDate.getMonth() + 1).padStart(2, '0');
    const day = String(localDate.getDate()).padStart(2, '0');
    const hours = String(localDate.getHours()).padStart(2, '0');
    const minutes = String(localDate.getMinutes()).padStart(2, '0');

    return `${year}-${month}-${day}T${hours}:${minutes}`;
}
