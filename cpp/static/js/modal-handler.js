// 格式化JSON字符串
import {showAlert} from './util.js';

function formatJSON(data) {
    try {
        if (typeof data === 'string') {
            const parsed = JSON.parse(data);
            return JSON.stringify(parsed, null, 2);
        }
        return JSON.stringify(data, null, 2);
    } catch (e) {
        return data || '';
    }
}

// 显示模态框
function showModal() {
    document.getElementById('modal').style.display = 'block';
    document.getElementById('modalOverlay').style.display = 'block';
}

// 关闭模态框
function closeModal() {
    document.getElementById('modal').style.display = 'none';
    document.getElementById('modalOverlay').style.display = 'none';
}

// 生成用户信息卡片HTML
function generateUserCardHTML(userInfo) {
    // 处理昵称和订单备注的显示
    const nickNameDisplay = userInfo.orderNote
        ? `${userInfo.nickName}（${userInfo.orderNote}）`
        : userInfo.nickName;

    // 处理渠道显示
    const channelDisplay = (() => {
        if (!userInfo.orderChannel) return '';
        const channelMap = {
            'taobao': '淘宝',
            'xianyu': '闲鱼',
            'weidian': '微店',
            'wechat': '微信',
            'other': '其他'
        };
        return channelMap[userInfo.orderChannel] || userInfo.orderChannel;
    })();

    return `
    <div class="user-card">
        <img src="${userInfo.headImage}" alt="头像" class="user-avatar">
        <div class="user-info">
            <div class="user-info-row">
                <span class="user-nickname">${nickNameDisplay}</span>
                <span class="user-channel">${channelDisplay}</span>
            </div>
            <div class="user-info-row">
                <span class="user-telephone">${userInfo.telephone}</span>
                <span class="user-sex">${userInfo.sex}</span>
            </div>
        </div>
    </div>`;
}

// 生成基本信息卡片HTML
function generateBasicInfoCardHTML(item, getTypeText, getStatusText) {
    return `
    <div class="card">
        <div class="card-row">
            <div class="card-item-horizontal">
                <p class="label">请求ID</p>
                <p class="value">${item.id}</p>
            </div>
            <div class="card-item-horizontal">
                <p class="label">设备ID</p>
                <p class="value">${item.deviceId}</p>
            </div>
        </div>
        <div class="card-row">
            <div class="card-item-horizontal">
                <span class="label">类型</span>
                <span class="value">${getTypeText(item.type)}</span>
            </div>
            <div class="card-item-horizontal">
                <span class="label">状态</span>
                <span class="value">${getStatusText(item.status)}</span>
            </div>
        </div>
        <div class="card-row">
            <div class="card-item-horizontal">
                <span class="label">数量</span>
                <span class="value">${item.quantity}件</span>
            </div>
            <div class="card-item-horizontal">
                <span class="label">收益</span>
                <span class="value">${item.actualEarnings}元</span>
            </div>
        </div>
        <div class="card-row">
            <div class="card-item-horizontal">
                <p class="label">延迟</p>
                <p class="value">${(item.delay / 1000).toFixed(1)} 秒</p>
            </div>
            <div class="card-item-horizontal">
                <p class="label">频率</p>
                <p class="value">${(item.frequency / 1000).toFixed(1)} 秒</p>
            </div>
        </div>
    </div>`;
}

// 生成时间信息卡片HTML
function generateTimeInfoCardHTML(item, formatDateWithoutYear, getBuyerText) {
    return `
    <div class="card">
        <div class="card-row">
            <div class="card-item">
                <p class="label">开售时间</p>
                <p class="value">${formatDateWithoutYear(new Date(item.startTime))}</p>
            </div>
            <div class="card-item">
                <p class="label">结束时间</p>
                <p class="value">${formatDateWithoutYear(new Date(item.endTime))}</p>
            </div>
        </div>
        <div class="card-row">
            <div class="card-item">
                <p class="label">线程ID</p>
                <p class="value">${item.threadId == null ? "未分配" : item.threadId}</p>
            </div>
            <div class="card-item">
                <p class="label">抢购者ID</p>
                <p class="value">${getBuyerText(item.buyerId)}</p>
            </div>
        </div>
    </div>`;
}

// 生成响应信息卡片HTML（仅结果页面使用）
function generateResponseCardHTML(responseMessage) {
    if (!responseMessage) return '';

    const status = responseMessage.status;
    return `
    <div class="card">
        <div class="card-row">
            <div class="card-item-horizontal">
                <p class="label">状态码</p>
                <p class="value">${status.code}</p>
            </div>
            <div class="card-item-horizontal">
                <p class="label">消息</p>
                <p class="value">${status.message}</p>
            </div>
        </div>
        <div class="card-row">
            ${responseMessage.count ? `
                <div class="card-item-horizontal">
                    <p class="label">次数</p>
                    <p class="value">${responseMessage.count}</p>
                </div>
            ` : ''}
            <div class="card-item-horizontal">
                <p class="label">描述</p>
                <p class="value">${status.description}</p>
            </div>
        </div>
    </div>`;
}

// 生成订单链接列表HTML（仅结果页面使用）
function generateOrderLinksHTML(orderLinkList) {
    if (!orderLinkList || orderLinkList.length === 0) return '';

    return `
    <div class="card">
        ${orderLinkList.map(order => `
            <div class="card-row-only">
                <div class="card-item-only">
                    <p class="label">添加时间</p>
                    <p class="value">${order.addTime}</p>
                </div>
                <div class="card-item-only">
                    <p class="label">标题</p>
                    <p class="value">${order.title}</p>
                </div>
                <div class="card-item-only">
                    <p class="label">描述</p>
                    <p class="value">${order.desc}</p>
                </div>
                <div class="card-item-only">
                    <p class="label">订单链接</p>
                    <div class="value-group value">
                        <a href="${order.orderLink}" target="_blank">点击查看</a>
                        <button class="modal-button secondary" onclick="copyToClipboard('${order.orderLink}')">复制</button>
                    </div>
                </div>
            </div>
        `).join('')}
    </div>`;
}

// 生成其他信息卡片HTML
function generateOtherInfoCardHTML(item) {
    return `
    <div class="card">
        <div class="card-item-horizontal">
            <p class="label">留言</p>
            <p class="value">${item.message}</p>
        </div>
        <div class="card-item-horizontal">
            <p class="label">身份证号</p>
            <p class="value">${item.idNumber}</p>
        </div>
        <div class="card-item-horizontal">
            <p class="label">商品关键词</p>
            <p class="value">${item.keyword == null ? "" : item.keyword}</p>
        </div>
    </div>`;
}

// 生成操作按钮HTML
function generateButtonsHTML(isRequest = false) {
    return `
        ${isRequest ? '<button class="modal-button danger" id="deleteButton">删除</button>' : ''}
        <button class="modal-button primary" id="copyInfoButton">复制信息</button>
    `;
}

// 显示详情
function showDetails(item, isRequest = false, {
    getTypeText = (type) => type,
    getStatusText = (status) => status,
    formatDateWithoutYear = (date) => date.toLocaleString(),
    getBuyerText = (id) => id
} = {}) {
    const userInfo = item.userInfo;
    const modalBody = document.getElementById('modal-body');
    const modalFooter = document.getElementById('modal-footer');
    const orderTemplateStr = formatJSON(item.orderTemplate);

    let html = '';
    html += generateUserCardHTML(userInfo);
    html += generateBasicInfoCardHTML(item, getTypeText, getStatusText);
    html += generateTimeInfoCardHTML(item, formatDateWithoutYear, getBuyerText);

    // 如果是结果页面，添加响应信息和订单链接
    if (!isRequest && item.responseMessage) {
        if (item.status !== 3) {
            html += generateResponseCardHTML(item.responseMessage);
            if (item.status === 1 && item.responseMessage.orderLink_list) {
                html += generateOrderLinksHTML(item.responseMessage.orderLink_list);
            }
        } else {
            html += `
            <span class="label">报错信息:</span>
            <div class="query-group">
                <input type="text" value='${formatJSON(item.responseMessage)}'>
                <button class="modal-button secondary" onclick="copyToClipboard('${JSON.stringify(item.responseMessage)}')">复制</button>
            </div>`;
        }
    }

    // 添加链接和Cookies输入框
    html += `
    <span class="label">链接:</span>
    <div class="query-group">
        <input type="text" value="${item.link}">
        <button class="modal-button secondary" onclick="openLink('${item.link}')">打开</button>
    </div>

    <span class="label">Cookies:</span>
    <div class="query-group">
        <input type="text" value="${item.cookies}">
        <button class="modal-button secondary" id="checkCookiesButton">检查</button>
    </div>

    <span class="label">下单模板:</span>
    <div class="query-group">
        <input type="text" value='${orderTemplateStr == "null" ? "" : orderTemplateStr}'>
        <button class="modal-button secondary" onclick="copyToClipboard('${item.orderTemplate}')">复制</button>
    </div>`;


    html += generateOtherInfoCardHTML(item);
    modalBody.innerHTML = html;

    // 设置底部按钮
    modalFooter.innerHTML = generateButtonsHTML(isRequest);

    showModal();

    // 设置按钮事件
    setupButtonEvents(item, userInfo);
}

// 设置按钮事件
function setupButtonEvents(item, userInfo) {
    // 删除按钮事件
    const deleteButton = document.getElementById('deleteButton');
    if (deleteButton) {
        deleteButton.onclick = function () {
            const password = prompt('请输入密码：1');
            if (password === "1") {
                fetch(`/api/delete/request/${item.id}`, {method: 'DELETE'})
                    .then(response => response.json())
                    .then(payload => {
                        if (payload.success) {
                            showAlert('删除成功', 'success');
                            closeModal();
                            document.querySelector(`.result-item[data-id="${item.id}"]`).remove();
                        } else {
                            showAlert(payload.error?.message || '删除失败', 'error');
                        }
                    })
                    .catch(error => console.error('删除失败:', error));
            } else {
                showAlert('密码错误', 'error');
            }
        };
    }

    // 复制信息按钮事件
    document.getElementById('copyInfoButton').onclick = function () {
        const copyData = {
            userInfo: {
                nickName: userInfo.nickName,
                telephone: userInfo.telephone
            },
            orderUrl: item.link,
            cookie: item.cookies
        };
        copyToClipboard(JSON.stringify(copyData));
    };

    // 检查Cookies按钮事件
    document.getElementById('checkCookiesButton').onclick = function () {
        fetch(`/api/get/cookies/validity?cookies=${encodeURIComponent(item.cookies)}`)
            .then(response => response.json())
            .then(payload => {
                if (payload.success) {
                    const message = payload.data?.message || 'Cookies 校验完成';
                    showAlert(message, 'info');
                } else {
                    showAlert(payload.error?.message || '检查失败', 'error');
                }
            })
            .catch(error => console.error('检查失败:', error));
    };
}

// 打开链接
function openLink(link) {
    window.open(link, '_blank');
}

// 复制到剪贴板
function copyToClipboard(text) {
    const textarea = document.createElement('textarea');
    textarea.value = text;
    document.body.appendChild(textarea);
    textarea.select();
    try {
        document.execCommand('copy');
        showAlert('复制成功', 'success');
    } catch (err) {
        console.error('复制失败:', err);
        showAlert('复制失败', 'error');
    }
    document.body.removeChild(textarea);
}

export {
    showDetails,
    showModal,
    closeModal,
    formatJSON,
    openLink,
    copyToClipboard
}; 