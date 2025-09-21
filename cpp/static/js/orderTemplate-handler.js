import {getInputValue} from './form-handler.js';
import {showAlert} from './util.js';

export {generateOrderTemplate, showModal};

window.getOrderTemplate = getOrderTemplate;
window.generateOrderTemplate = generateOrderTemplate;

function getOrderTemplate() {
    const data = getInputValue();
    if (data.cookies == null || data.cookies === '' || data.link == null || data.link === '') {
        showAlert('请先填写cookies或链接', 'warning');
        return;
    }

    fetch('/getNote', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify(data),
    })
        .then(response => {
            if (!response.ok) {
                return response.json().then(errorData => {
                    throw new Error(errorData.status.description);
                });
            }
            return response.json();
        })
        .then(data => {
            const status = data.status;
            const result = data.result;

            if (status.code === 200) {
                if (Object.keys(result.custom_info).length !== 0) {
                    document.getElementById('orderTemplate').value = JSON.stringify(result.custom_info, null, 2);
                    showAlert('查询备注成功', 'success');
                    generateOrderTemplate();
                } else {
                    document.getElementById('orderTemplate').value = '';
                    showAlert('备注为空', 'info');
                }
            } else {
                showAlert(status.description, 'error');
            }
        })
        .catch(error => showAlert('查询备注失败: ' + error.message, 'error'));
}

window.closeModal = closeModal;
window.showModal = showModal;

function showModal() {
    document.getElementById('modal').style.display = 'block';
    document.getElementById('modalOverlay').style.display = 'block';
}

function closeModal() {
    document.getElementById('modal').style.display = 'none';
    document.getElementById('modalOverlay').style.display = 'none';
}

function generateOrderTemplate() {
    var data = document.getElementById('orderTemplate').value;
    if (data == '') {
        showAlert('请先填写备注信息', 'warning');
        return;
    }
    try {
        data = JSON.parse(data);
        const container = document.getElementById('formContainer');
        container.innerHTML = ''; // 清空现有表单
        data.forEach(item => {
            const formGroup = document.createElement('div');
            formGroup.className = 'form-group';

            const label = document.createElement('label');
            label.textContent = item.name;
            label.htmlFor = item.name;
            if (item.required === 1) {
                const star = document.createElement('span');
                star.textContent = '*';
                star.style.color = 'red';
                label.appendChild(star);
            }

            let inputElement;
            if (item.format === 'choice') {
                inputElement = document.createElement('select');
                inputElement.multiple = item.choice_type === 2; // 多选
                item.choice_list.forEach(choice => {
                    const option = document.createElement('option');
                    option.value = choice.name;
                    option.textContent = choice.name;
                    inputElement.appendChild(option);
                });
            } else if (item.format === 'pic') {
                inputElement = document.createElement('input');
                inputElement.type = 'file';
                inputElement.accept = 'image/*';
                inputElement.multiple = item.max_pics > 1;
                inputElement.onchange = function () {
                    handleImageUpload(inputElement, item);
                };
            } else {
                inputElement = document.createElement('input');
                inputElement.type = item.format === 'num' ? 'number' :
                    item.format === 'mobile' ? 'tel' :
                        item.format === 'email' ? 'email' :
                            item.format === 'time' ? 'time' :
                                item.format === 'date' ? 'date' : 'text';
                inputElement.placeholder = item.def_value;
                inputElement.value = item.value || '';
            }
            inputElement.id = item.name;
            inputElement.name = item.name;
            inputElement.required = item.required === 1;

            formGroup.appendChild(label);
            formGroup.appendChild(inputElement);
            container.appendChild(formGroup);

            // 创建一个用于显示图片预览的容器
            if (item.format === 'pic') {
                const previewContainer = document.createElement('div');
                previewContainer.className = 'image-preview-container';
                formGroup.appendChild(previewContainer);

                // 如果已经有上传的图片，显示它们
                if (item.value) {
                    const imageUrls = item.value.split('|');
                    imageUrls.forEach(url => {
                        if (url) {
                            displayImageURL(inputElement, url);
                        }
                    });
                }
            }
        });

        showModal(); // 只有在成功生成表单后才显示模态框
    } catch (e) {
        showAlert('备注信息格式不正确，请确保是有效的JSON格式', 'error');
        return;
    }
}

// 将 validateAndCollectFormData 改为 window.submitForm
window.submitForm = function () {
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
};

function collectFormData() {
    var jsonData = document.getElementById('orderTemplate').value;
    if (jsonData == '') {
        alert('请先填写备注信息');
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
                    self_choice: selectedOptions.reduce((acc, curr) => ({ ...acc, [curr]: true }), {}),
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
                return { ...item, value: imgSrc };
            } else {
                newValue = input.value;
                return { ...item, value: newValue };
            }
        }
        return item;
    });

    document.getElementById('orderTemplate').value = JSON.stringify(updatedData);
    console.log('收集好的数据:', updatedData);
    closeModal();
}

function handleImageUpload(input, item) {
    const files = input.files;
    if (files.length > (item.max_pics || 1)) {
        showAlert(`最多只能上传 ${item.max_pics || 1} 张图片`, 'warning');
        input.value = ''; // 清空文件选择
        return;
    }
    showAlert('正在上传图片，请稍候', 'info')
    const requestData = getInputValue();
    const cookies = requestData.cookies;
    Array.from(files).forEach(file => {
        const formData = new FormData();
        formData.append('file', file);
        formData.append('customCookies', cookies);
        fetch('/api/upload', {
            method: 'POST',
            body: formData,
        })
            .then(response => response.json())
            .then(data => {
                if (data && data.result && data.result.url) {
                    showAlert('上传成功', 'success')
                    console.log('上传成功', data);
                    item.value = data.result.url;
                    displayImageURL(input, data.result.url);
                } else {
                    showAlert('上传失败', 'error');
                }
            })
            .catch(error => {
                showAlert('上传错误: ' + error.message, 'error');
            });
    });
}

function displayImageURL(inputElement, url) {
    const previewContainer = inputElement.parentNode.querySelector('.image-preview-container');

    // 检查是否已经存在相同的图片
    const existingImages = previewContainer.querySelectorAll('img');
    for (let img of existingImages) {
        if (img.src === url) {
            return; // 如果图片已存在，不重复添加
        }
    }

    const imageWrapper = document.createElement('div');
    imageWrapper.className = 'image-wrapper';
    imageWrapper.style.position = 'relative';

    const imagePreview = document.createElement('img');
    imagePreview.src = url;
    imagePreview.alt = '图片预览';

    // 添加删除按钮
    const deleteButton = document.createElement('button');
    deleteButton.innerHTML = '×';
    deleteButton.style.position = 'absolute';
    deleteButton.style.top = '-8px';
    deleteButton.style.right = '-8px';
    deleteButton.style.backgroundColor = '#ff4d4f';
    deleteButton.style.color = 'white';
    deleteButton.style.border = 'none';
    deleteButton.style.borderRadius = '50%';
    deleteButton.style.width = '20px';
    deleteButton.style.height = '20px';
    deleteButton.style.cursor = 'pointer';
    deleteButton.style.display = 'flex';
    deleteButton.style.alignItems = 'center';
    deleteButton.style.justifyContent = 'center';
    deleteButton.style.fontSize = '14px';
    deleteButton.style.padding = '0';
    deleteButton.style.lineHeight = '1';

    deleteButton.onclick = function (e) {
        e.preventDefault();
        imageWrapper.remove();
        updateImageValue(inputElement);
    };

    imageWrapper.appendChild(imagePreview);
    imageWrapper.appendChild(deleteButton);
    previewContainer.appendChild(imageWrapper);

    updateImageValue(inputElement);
}

function updateImageValue(inputElement) {
    const previewContainer = inputElement.parentNode.querySelector('.image-preview-container');
    const images = previewContainer.querySelectorAll('img');
    const imageUrls = Array.from(images).map(img => img.src).join('|');

    // 更新表单数据中的图片值
    const data = JSON.parse(document.getElementById('orderTemplate').value);
    const item = data.find(item => item.name === inputElement.name);
    if (item) {
        item.value = imageUrls;
        document.getElementById('orderTemplate').value = JSON.stringify(data);
    }
}
