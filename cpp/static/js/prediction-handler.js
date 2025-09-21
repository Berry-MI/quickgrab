// 预测规则处理
export class PredictionHandler {
    constructor() {
        this.predictMode = document.getElementById('predictMode');
        this.setNoteButton = document.getElementById('setNoteButton');
        this.orderTemplate = document.getElementById('orderTemplate');
        this.rules = [];

        this.init();
    }

    init() {
        // 监听预测模式切换
        this.predictMode.addEventListener('change', () => {
            this.updateButtonText();
        });
    }

    updateButtonText() {
        this.setNoteButton.textContent = this.predictMode.checked ? '填写规则' : '填写信息';
    }

    // 显示预测规则编辑模态框
    showPredictionRulesModal() {
        const formContainer = document.getElementById('formContainer');
        formContainer.innerHTML = this.generateRulesForm();

        // 添加新规则按钮的事件监听
        const addButton = document.getElementById('addRuleButton');
        if (addButton) {
            addButton.addEventListener('click', () => this.addNewRule());
        }

        // 显示模态框
        document.getElementById('modal').style.display = 'block';
        document.getElementById('modalOverlay').style.display = 'block';
    }

    // 生成规则编辑表单
    generateRulesForm() {
        let currentRules = [];
        try {
            if (this.orderTemplate.value) {
                currentRules = JSON.parse(this.orderTemplate.value);
            }
        } catch (e) {
            currentRules = [];
        }

        // 查找AI规则
        const aiRule = currentRules.find(rule => rule.keyword === 'AI规则');
        const aiRuleValue = aiRule ? aiRule.value : '';

        let html = `
            <div class="rules-container">
                <div class="rules-header">
                    <h3>预测规则设置</h3>
                    <p>添加可能出现的输入提示关键词及需输入的值</p>
                </div>
                <div class="ai-rule-container">
                    <div class="rule-item ai-rule">
                        <div class="rule-inputs">
                            <div class="input-group">
                                <input type="text" class="rule-keyword" value="AI规则" readonly style="background: #f5f5f5;">
                                <input type="text" class="rule-value ai-rule-value" placeholder="AI参考规则" value="${aiRuleValue}">
                            </div>
                        </div>
                    </div>
                    <p class="ai-rule-hint">AI将参考此规则生成答案</p>
                </div>
                <div id="rulesList">
        `;

        // 添加现有规则（排除AI规则）
        currentRules.filter(rule => rule.keyword !== 'AI规则').forEach((rule, index) => {
            html += this.generateRuleHTML(rule, index);
        });

        html += `
                </div>
                <button type="button" id="addRuleButton" class="add-rule-button">
                    <i class="ri-add-line"></i> 添加规则
                </button>
            </div>
        `;

        return html;
    }

    // 生成单个规则的HTML
    generateRuleHTML(rule = {keyword: '', value: ''}, index) {
        return `
            <div class="rule-item" data-index="${index}">
                <div class="rule-inputs">
                    <div class="input-group">
                        <input type="text" class="rule-keyword" placeholder="关键词" value="${rule.keyword}">
                        <input type="text" class="rule-value" placeholder="填入值" value="${rule.value}">
                    </div>
                </div>
                <button type="button" class="delete-rule-button" onclick="window.predictionHandler.deleteRule(${index})">
                    <i class="ri-delete-bin-line"></i>
                </button>
            </div>
        `;
    }

    // 添加新规则
    addNewRule() {
        const rulesList = document.getElementById('rulesList');
        const newIndex = rulesList.children.length;
        const newRuleHTML = this.generateRuleHTML({keyword: '', value: ''}, newIndex);

        // 将新规则HTML添加到列表末尾
        const tempDiv = document.createElement('div');
        tempDiv.innerHTML = newRuleHTML;
        rulesList.appendChild(tempDiv.firstElementChild);
    }

    // 删除规则
    deleteRule(index) {
        const ruleItem = document.querySelector(`.rule-item[data-index="${index}"]`);
        if (ruleItem) {
            ruleItem.remove();
            // 重新排序索引
            document.querySelectorAll('.rule-item').forEach((item, idx) => {
                item.dataset.index = idx;
            });
        }
    }

    // 保存规则
    saveRules() {
        const rules = [];

        // 首先保存AI规则
        const aiRuleValue = document.querySelector('.ai-rule-value').value.trim();
        if (aiRuleValue) {
            rules.push({keyword: 'AI规则', value: aiRuleValue});
        }

        // 保存其他规则
        document.querySelectorAll('.rule-item:not(.ai-rule)').forEach(item => {
            const keyword = item.querySelector('.rule-keyword').value.trim();
            const value = item.querySelector('.rule-value').value.trim();
            if (keyword && value) {
                rules.push({keyword, value});
            }
        });

        // 将规则保存为JSON字符串
        this.orderTemplate.value = JSON.stringify(rules);
        this.closeModal();
    }

    // 关闭模态框
    closeModal() {
        document.getElementById('modal').style.display = 'none';
        document.getElementById('modalOverlay').style.display = 'none';
    }
}

// 创建全局实例
window.predictionHandler = new PredictionHandler(); 