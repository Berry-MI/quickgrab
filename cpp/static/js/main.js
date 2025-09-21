import {pasteCookiesAndLink} from './clipboard-handler.js';
import {generateOrderTemplate} from './orderTemplate-handler.js';
import {showAlert} from './util.js';

document.addEventListener('DOMContentLoaded', function () {
    document.querySelector('.paste-button').addEventListener('click', pasteCookiesAndLink);
});

// 处理"填写信息"按钮点击
window.handleSetNoteClick = function () {
    const predictMode = document.getElementById('predictMode');
    if (predictMode.checked) {
        // 如果是预测模式，显示预测规则编辑界面
        window.predictionHandler.showPredictionRulesModal();
    } else {
        // 否则显示普通的填写信息界面
        const orderTemplate = document.getElementById('orderTemplate').value;
        if (!orderTemplate) {
            showAlert('请先填写备注信息', 'warning');
            return;
        }
        try {
            JSON.parse(orderTemplate); // 验证JSON格式
            generateOrderTemplate();
        } catch (e) {
            showAlert('备注信息格式不正确，请确保是有效的JSON格式', 'error');
        }
    }
};

// 处理模态框的提交
window.submitForm = function () {
    const predictMode = document.getElementById('predictMode');
    if (predictMode.checked) {
        // 如果是预测模式，保存预测规则
        window.predictionHandler.saveRules();
    } else {
        // 否则执行原有的提交逻辑
        const container = document.getElementById('formContainer');
        const inputs = container.querySelectorAll('input, select');
        let isValid = true;
        let firstInvalidElement = null;

        inputs.forEach(input => {
            if (input.required && !input.value) {
                isValid = false;
                firstInvalidElement = firstInvalidElement || input;
                input.style.borderColor = 'red';
            } else {
                input.style.borderColor = '';
            }
        });

        if (isValid) {
            collectFormData();
        } else {
            showAlert('请填写所有必填项', 'warning');
            firstInvalidElement.focus();
        }
    }
};

function collectFormData() {
    var jsonData = document.getElementById('orderTemplate').value;
    if (jsonData == '') {
        showAlert('请先填写备注信息', 'warning');
        return;
    }
    jsonData = JSON.parse(jsonData);
    const container = document.getElementById('formContainer');
    const inputs = container.querySelectorAll('input, select');
    const updatedData = jsonData.map(item => {
        const input = Array.from(inputs).find(input => input.getAttribute('name') === item.name);
        if (input) {
            let newValue;
            if (item.format === 'choice') {
                const selectedOptions = Array.from(input.selectedOptions).map(option => option.value);
                newValue = item.choice_type === 1 ? selectedOptions[0] : selectedOptions.join(',');
                return {
                    ...item,
                    value: newValue,
                    self_choice: selectedOptions.reduce((acc, curr) => ({...acc, [curr]: true}), {}),
                    choice_value_list: selectedOptions
                };
            } else if (item.format === 'pic') {
                const imagePreviews = input.parentNode.querySelectorAll('img');
                let imgSrc = '';
                imagePreviews.forEach(img => {
                    if (img.src !== '') {
                        imgSrc += img.src + "|";
                    }
                });
                imgSrc = imgSrc.substring(0, imgSrc.length - 1);
                return {...item, value: imgSrc};
            } else {
                newValue = input.value;
                return {...item, value: newValue};
            }
        }
        return item;
    });

    document.getElementById('orderTemplate').value = JSON.stringify(updatedData);
    console.log('收集好的数据:', updatedData);
    closeModal();
}

function closeModal() {
    document.getElementById('modal').style.display = 'none';
    document.getElementById('modalOverlay').style.display = 'none';
}
