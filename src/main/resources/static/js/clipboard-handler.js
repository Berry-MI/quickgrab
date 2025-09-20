// clipboard-handler.js
import {parseCookiesAndLink} from './form-handler.js';
import {showAlert} from './util.js';
export async function pasteCookiesAndLink() {
    try {
        pasteFromClipboard('cookiesAndLink');
        await parseCookiesAndLink.call(document.getElementById('cookiesAndLink'));
    } catch (err) {
        console.error('Failed to read clipboard contents: ', err);
        showAlert('无法读取剪切板内容，请手动粘贴。');
    }
}


export function pasteFromClipboard(inputId) {
    const inputElement = document.getElementById(inputId);

    if (inputElement) {
        try {
            const text = AndroidClipboard.getText(); // 调用 Java 方法获取剪贴板内容
            inputElement.value = text;
            console.log('Pasted from clipboard:', text);
        } catch (err) {
            console.error('Failed to read clipboard contents: ', err);
            showAlert('粘贴失败，请手动粘贴');
        }
    } else {
        console.error(`没有找到ID为 ${inputId} 的输入框`);
    }
}

export function copyToClipboard(text) {
    if (navigator.clipboard && navigator.clipboard.writeText) {
        navigator.clipboard.writeText(text).then(() => {
            showAlert('已复制到剪切板');
        }).catch(err => {
            console.error('Failed to copy: ', err);
            showAlert('复制失败，请手动复制');
        });
    } else {
        // 在navigator.clipboard不受支持的环境中的回退方法
        const textArea = document.createElement("textarea");
        textArea.value = text;
        // 避免在页面上显示文本框
        textArea.style.position = 'fixed';
        document.body.appendChild(textArea);
        textArea.select();
        try {
            document.execCommand('copy');
            showAlert('已复制到剪切板');
        } catch (err) {
            console.error('Failed to copy using fallback method: ', err);
            showAlert('复制失败，请手动复制');
        }
        document.body.removeChild(textArea);
    }
}

window.pasteCookiesAndLink = pasteCookiesAndLink;
window.pasteFromClipboard = pasteFromClipboard;
window.copyToClipboard = copyToClipboard;