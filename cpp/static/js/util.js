// 自定义弹窗
export function showAlert(message, type = 'info') {
    const alertBox = document.getElementById('customAlert');
    alertBox.textContent = message;

    // 移除所有可能的类型类名
    alertBox.classList.remove('success', 'error', 'warning', 'info');

    // 添加新的类型类名
    alertBox.classList.add(type);
    alertBox.classList.add('show');

    // 2秒后隐藏提示框
    setTimeout(() => {
        alertBox.classList.remove('show');
    }, 2000);
}

export function getTextContentByValue(value,selectId) {
    const select = document.getElementById(selectId);
    const options = select.options;
    for (let i = 0; i < options.length; i++) {
        if (options[i].value === value) {
            return options[i].textContent;
        }
    }
    return 'null';
}

export function throttle(fn, wait) {
    let lastTime = 0;
    return function () {
        const now = Date.now();
        if (now - lastTime >= wait) {
            lastTime = now;
            fn.apply(this, arguments);
        }
    };
}

// 设置按钮加载状态
export function setButtonLoading(button, isLoading, loadingText = '加载中...', originalText = null) {
    if (isLoading) {
        button.disabled = true;
        button._originalText = button.innerHTML;
        button.innerHTML = `<i class="ri-loader-4-line ri-spin"></i> ${loadingText}`;
    } else {
        button.disabled = false;
        button.innerHTML = originalText || button._originalText;
    }
}

window.showAlert = showAlert;
window.getTextContentByValue = getTextContentByValue;
window.throttle = throttle;