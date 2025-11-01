// 列表处理器类
class ListHandler {
    constructor(options) {
        this.options = {
            containerId: '',
            fetchUrl: '',
            isRequest: false,
            limit: 15,
            ...options
        };

        this.offset = 0;
        this.loading = false;

        // 绑定事件处理器
        this.bindEvents();

        // 初始化
        this.init();
    }

    // 初始化
    init() {
        this.fetchBuyers()
            .then(() => {
                this.fetchItems();
            })
            .catch(error => console.error('Error during initialization:', error));
    }

    // 绑定事件
    bindEvents() {
        // 绑定筛选条件变化事件
        ['buyerSelect', 'type', 'status', 'order'].forEach(id => {
            const element = document.getElementById(id);
            if (element) {
                element.onchange = () => this.resetAndFetch();
            }
        });

        // 绑定滚动事件
        const container = document.getElementById(this.options.containerId);
        if (container) {
            container.addEventListener('scroll', () => this.checkScroll());
        }
    }

    // 获取买家列表
    async fetchBuyers() {
        try {
            const userResponse = await fetch('/api/get/user');
            const userPayload = await userResponse.json();
            if (!userPayload.success || !userPayload.data) {
                throw new Error('无法获取用户信息');
            }
            const currentAccount = userPayload.data.account || userPayload.data;

            // 显示/隐藏买家选择器
            const buyerSelect = document.getElementById('buyerSelect');
            if ((currentAccount.accessLevel || currentAccount.privilegeLevel || 0) > 3) {
                buyerSelect.style.display = 'block';
            } else {
                buyerSelect.style.display = 'none';
            }

            const currentUser = currentAccount.username || currentAccount.displayName;
            const buyersResponse = await fetch('/api/get/buyers');
            const buyersPayload = await buyersResponse.json();
            if (!buyersPayload.success || !buyersPayload.data) {
                throw new Error('无法获取买家列表');
            }
            const buyersData = buyersPayload.data.buyers || buyersPayload.data.items || [];

            // 填充买家选择器
            buyerSelect.innerHTML = '<option value="">所有买家</option>';
            buyersData.forEach(buyer => {
                const option = document.createElement('option');
                option.value = buyer.id;
                option.textContent = buyer.username;
                if (buyer.username === currentUser) {
                    option.selected = true;
                }
                buyerSelect.appendChild(option);
            });
        } catch (error) {
            console.error('Error fetching buyers:', error);
        }
    }

    // 重置并获取数据
    resetAndFetch() {
        this.offset = 0;
        document.getElementById(this.options.containerId).innerHTML = '';
        this.fetchItems();
    }

    // 获取筛选参数
    getQueryParams() {
        return {
            buyerId: document.getElementById('buyerSelect').value,
            type: document.getElementById('type').value,
            status: document.getElementById('status').value,
            keyword: document.getElementById('keywordInput').value,
            order: document.getElementById('order').value,
            offset: this.offset,
            limit: this.options.limit
        };
    }

    // 获取列表数据
    async fetchItems() {
        if (this.loading) return;
        this.loading = true;

        try {
            const query = new URLSearchParams(this.getQueryParams());
            const response = await fetch(`${this.options.fetchUrl}?${query.toString()}`);
            const payload = await response.json();
            if (!payload.success) {
                throw new Error(payload.error?.message || '查询失败');
            }
            const dataset = payload.data?.items || payload.data?.pending || payload.data || [];
            this.renderItems(dataset);

            this.offset += this.options.limit;
            this.loading = false;
        } catch (error) {
            console.error('查询失败:', error);
            this.loading = false;
        }
    }

    // 检查滚动位置
    checkScroll() {
        const container = document.getElementById(this.options.containerId);
        if (!container) return;

        // 添加一个小的缓冲区，提前触发加载
        const buffer = 50;
        const scrollPosition = container.scrollTop + container.clientHeight;
        const totalHeight = container.scrollHeight;

        // 当滚动到底部前50px时触发加载
        if (totalHeight - scrollPosition <= buffer) {
            this.fetchItems();
        }
    }

    // 渲染列表项
    renderItems(data) {
        if (!data || !Array.isArray(data) || data.length === 0) return;

        const container = document.getElementById(this.options.containerId);
        if (!container) return;

        const fragment = document.createDocumentFragment();

        data.forEach((item, index) => {
            const userInfo = item.userInfo || {};
            const statusText = this.options.getStatusText(item.status);
            const typeText = this.options.getTypeText(item.type);

            // 计算序号
            const itemIndex = this.offset + index + 1;

            // 处理昵称和订单备注的显示
            const nickName = userInfo.nickName || '未知';
            const orderNote = userInfo.orderNote;
            const telephone = userInfo.telephone || '未知';
            const nickNameDisplay = orderNote
                ? `${nickName}-${orderNote}`
                : nickName;

            const itemElement = document.createElement('div');
            itemElement.className = 'result-item';
            itemElement.setAttribute('data-id', item.id);
            itemElement.innerHTML = `
                <div class="item-container">
                    <div class="item-index">${itemIndex}</div>
                    <div class="item-content">
                        <div class="info">
                            <span class="telephone-nickName">${telephone} (${nickNameDisplay})</span>
                            <span class="status-indicator ${this.options.getStatusClass(item.status)}">${statusText}</span>
                        </div>
                        <div class="info">
                            <span class="type-info">${typeText}</span>
                            <span class="end-time">${this.options.formatDate(new Date(this.options.isRequest ? item.startTime : item.endTime))}</span>
                        </div>
                    </div>
                </div>
            `;

            // 使用事件委托来处理点击事件
            itemElement.addEventListener('click', (e) => {
                if (e.target && !e.target.closest('.no-details')) {
                    window.showDetails(item);
                }
            });

            fragment.appendChild(itemElement);
        });

        // 一次性添加所有元素到DOM
        container.appendChild(fragment);

        // 如果是首次加载（offset为0）且有数据，滚动到顶部
        if (this.offset === 0 && data.length > 0) {
            container.scrollTop = 0;
        }
    }
}

// 工具函数
function formatDateWithoutYear(date) {
    const formatter = new Intl.DateTimeFormat('zh-CN', {
        month: 'long',
        day: 'numeric',
        hour: '2-digit',
        minute: '2-digit',
        second: '2-digit'
    });
    return formatter.format(date);
}

function formatDateWithYear(date) {
    const formatter = new Intl.DateTimeFormat('zh-CN', {
        year: 'numeric',
        month: 'long',
        day: 'numeric',
        hour: '2-digit',
        minute: '2-digit',
        second: '2-digit'
    });
    return formatter.format(date);
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

function getBuyerText(id) {
    const buyerSelect = document.getElementById('buyerSelect');
    const options = buyerSelect.options;
    for (let i = 0; i < options.length; i++) {
        if (options[i].value == id) {
            return options[i].textContent;
        }
    }
    return 'null';
}

export {
    ListHandler,
    formatDateWithoutYear,
    formatDateWithYear,
    getTypeText,
    getBuyerText
}; 