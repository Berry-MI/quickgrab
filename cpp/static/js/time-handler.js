// time-handler.js

document.addEventListener("DOMContentLoaded", function() {
    fetchBuyers()
        .then(() => {
            setDefaultTimes();
            setupEventListeners();
        })
        .catch(error => console.error('Error during initialization:', error));
});
function fetchBuyers() {
    return fetch('/api/get/user')
        .then(response => response.json())
        .then(payload => {
            if (!payload.success || !payload.data) {
                throw new Error('无法获取用户信息');
            }
            const account = payload.data.account || payload.data;

            // 设定邮箱地址
            const emailField = document.getElementById('email');
            emailField.value = account.email || '';

            if ((account.accessLevel || account.privilegeLevel || 0) > 3) {
                document.getElementById('buyerSelectList').style.display = 'flex';
            } else {
                document.getElementById('buyerSelectList').style.display = 'none';
            }

            let currentUser = account.username || account.displayName;
            return fetch('/api/get/buyers').then(response => ({ response, currentUser }));
        })
        .then(({ response, currentUser }) => response.json().then(data => ({ data, currentUser })))
        .then(({ data, currentUser }) => {
            if (!data.success || !data.data) {
                throw new Error('无法获取买家列表');
            }
            const buyers = data.data.buyers || data.data.items || [];
            const buyerSelect = document.getElementById('buyerSelect');
            buyerSelect.innerHTML = '<option value="">所有买家</option>';
            buyers.forEach(buyer => {
                const option = document.createElement('option');
                option.value = buyer.id;
                option.textContent = buyer.username;
                if (buyer.username === currentUser) {
                    option.selected = true;
                }
                buyerSelect.appendChild(option);
            });
        })
        .catch(error => console.error('Error fetching buyers:', error));
}


function setDefaultTimes() {
    const now = new Date();

    // 获取当前 UTC 时间
    const utcNow = now.getTime() + now.getTimezoneOffset() * 60000;

    // 将时间转换为北京时间 (UTC+8)
    const beijingOffset = 8 * 60 * 60000;
    const beijingTime = new Date(utcNow + beijingOffset);

    // 设置开售时间为当前时间的下一分钟，并将秒数和毫秒数设置为0
    beijingTime.setMinutes(beijingTime.getMinutes() + 1);
    beijingTime.setSeconds(0);
    beijingTime.setMilliseconds(0);

    // 复制开售时间
    const startTime = new Date(beijingTime);

    // 设置结束时间为开售时间的10分钟后
    const endTime = new Date(startTime);
    endTime.setMinutes(startTime.getMinutes() + 10);

    // 格式化时间为YYYY-MM-DDTHH:MM
    const formattedStartTime = formatDateTime(startTime);
    const formattedEndTime = formatDateTime(endTime);

    // 设置表单中的默认值
    document.getElementById('startTime').value = formattedStartTime;
    document.getElementById('endTime').value = formattedEndTime;
}

function formatDateTime(date) {
    const year = date.getFullYear();
    const month = String(date.getMonth() + 1).padStart(2, '0');
    const day = String(date.getDate()).padStart(2, '0');
    const hours = String(date.getHours()).padStart(2, '0');
    const minutes = String(date.getMinutes()).padStart(2, '0');
    return `${year}-${month}-${day}T${hours}:${minutes}`;
}



function setupEventListeners() {
    document.getElementById('startTime').addEventListener('input', function() {
        updateEndTime();
    });
}

function updateEndTime() {
    const startTimeInput = document.getElementById('startTime').value;
    if (startTimeInput) {
        const startTime = new Date(startTimeInput);
        // 设置结束时间为开售时间的10分钟后
        const endTime = new Date(startTime);
        endTime.setMinutes(startTime.getMinutes() + 10);

        // 格式化时间为YYYY-MM-DDTHH:MM
        const formattedEndTime = formatDateTime(endTime);

        // 设置表单中的默认值
        document.getElementById('endTime').value = formattedEndTime;
    }
}