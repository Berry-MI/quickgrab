document.addEventListener('DOMContentLoaded', function () {
    fetchBuyers()
        .then(() => {
            updateSelectedTimeRangeText();
            updateStatistics();
            updateCharts();
            setupStatusFilter();
            setupCustomTimeRangeListeners();
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
            if ((account.accessLevel || account.privilegeLevel || 0) > 3) {
                document.getElementById('buyerSelectList').style.display = 'block';
            } else {
                document.getElementById('buyerSelectList').style.display = 'none';
            }

            let currentUser = account.username || account.displayName;
            return fetch('/api/get/buyers').then(response => ({ response, currentUser }));
        })
        .then(({ response, currentUser }) => response.json().then(data => ({ data, currentUser })))
        .then(({ data, currentUser }) => {
            if (!data.success || !data.data) {
                throw new Error('无法加载买家列表');
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


function updateStatistics() {
    const buyerId = document.getElementById('buyerSelect').value;
    const status = document.getElementById('statusFilter').value;
    const timeRange = document.getElementById('timeRange').value;
    const startTime = document.getElementById('startTime').value;
    const endTime = document.getElementById('endTime').value;

    let start = null;
    let end = null;

    switch (timeRange) {
        case 'today':
            start = new Date();
            start.setHours(0, 0, 0, 0);
            end = new Date();
            end.setHours(23, 59, 59, 999);
            break;
        case 'yesterday':
            start = new Date();
            start.setDate(start.getDate() - 1);
            start.setHours(0, 0, 0, 0);
            end = new Date();
            end.setDate(end.getDate() - 1);
            end.setHours(23, 59, 59, 999);
            break;
        case 'thisMonth':
            start = new Date();
            start.setDate(1);
            start.setHours(0, 0, 0, 0);
            end = new Date();
            end.setHours(23, 59, 59, 999);
            break;
        case 'lastMonth':
            start = new Date();
            start.setMonth(start.getMonth() - 1);
            start.setDate(1);
            start.setHours(0, 0, 0, 0);
            end = new Date();
            end.setMonth(end.getMonth() - 1);
            end.setDate(new Date(end.getFullYear(), end.getMonth() + 1, 0).getDate());
            end.setHours(23, 59, 59, 999);
            break;
        case 'last15Days':
            start = new Date();
            start.setDate(start.getDate() - 15);
            start.setHours(0, 0, 0, 0);
            end = new Date();
            end.setHours(23, 59, 59, 999);
            break;
        case 'last30Days':
            start = new Date();
            start.setDate(start.getDate() - 30);
            start.setHours(0, 0, 0, 0);
            end = new Date();
            end.setHours(23, 59, 59, 999);
            break;
        case 'custom':
            if (startTime && endTime) {
                start = new Date(startTime);
                end = new Date(endTime);
            }
            break;
        default:
            break;
    }

    const startTimeStr = start ? start.toISOString() : '';
    const endTimeStr = end ? end.toISOString() : '';

    fetch(`/api/get/statistics?buyerId=${buyerId}&status=${status}&startTime=${startTimeStr}&endTime=${endTimeStr}`)
        .then(response => response.json())
        .then(payload => {
            if (!payload.success || !payload.data) {
                throw new Error('统计数据获取失败');
            }
            updateTable(payload.data.typeStats || []);
        })
        .catch(error => console.error('Error fetching statistics:', error));
}

function updateTable(typeStats) {
    const rows = {
        1: {
            success: document.getElementById('timedSaleSuccess'),
            failure: document.getElementById('timedSaleFailure'),
            exception: document.getElementById('timedSaleException'),
            total: document.getElementById('timedSaleTotal')
        },
        2: {
            success: document.getElementById('manualSaleSuccess'),
            failure: document.getElementById('manualSaleFailure'),
            exception: document.getElementById('manualSaleException'),
            total: document.getElementById('manualSaleTotal')
        },
        3: {
            success: document.getElementById('snatchModeSuccess'),
            failure: document.getElementById('snatchModeFailure'),
            exception: document.getElementById('snatchModeException'),
            total: document.getElementById('snatchModeTotal')
        },
        'total': {
            success: document.getElementById('totalSuccess'),
            failure: document.getElementById('totalFailure'),
            exception: document.getElementById('totalException'),
            total: document.getElementById('totalCount')
        }
    };

    let totalSuccess = 0;
    let totalFailure = 0;
    let totalException = 0;
    let totalCount = 0;
    let totalSuccessEarnings = 0;
    let totalFailureEarnings = 0;
    let totalExceptionEarnings = 0;
    let totalEarnings = 0;

    typeStats.forEach(stat => {
        const type = stat.type === 'total' ? 'total' : parseInt(stat.type);
        rows[type].success.textContent = stat.successCount || 0;
        rows[type].failure.textContent = stat.failureCount || 0;
        rows[type].exception.textContent = stat.exceptionCount || 0;
        rows[type].total.textContent = stat.totalCount || 0;

        if (type !== 'total') {
            totalSuccess += stat.successCount || 0;
            totalFailure += stat.failureCount || 0;
            totalException += stat.exceptionCount || 0;
            totalCount += stat.totalCount || 0;
        }

        if (type === 'total') {
            totalSuccessEarnings = stat.successEarnings || 0;
            totalFailureEarnings = stat.failureEarnings || 0;
            totalExceptionEarnings = stat.exceptionEarnings || 0;
            totalEarnings = stat.totalEarnings || 0;
        }
    });

    document.getElementById('totalSuccess').textContent = totalSuccess;
    document.getElementById('totalFailure').textContent = totalFailure;
    document.getElementById('totalException').textContent = totalException;
    document.getElementById('totalCount').textContent = totalCount;

    document.getElementById('successEarnings').textContent = totalSuccessEarnings;
    document.getElementById('failureEarnings').textContent = totalFailureEarnings;
    document.getElementById('exceptionEarnings').textContent = totalExceptionEarnings;
    document.getElementById('totalEarnings').textContent = totalEarnings;
}

function setCustomTimeRange() {
    const timeRange = document.getElementById('timeRange').value;
    const customTimeRangeDiv = document.getElementById('customTimeRange');
    const selectedTimeRangeText = document.getElementById('selectedTimeRange');
    
    if (timeRange === 'custom') {
        customTimeRangeDiv.style.display = 'block';
        updateSelectedTimeRangeText();
    } else {
        customTimeRangeDiv.style.display = 'none';
        updateStatistics();
        updateSelectedTimeRangeText();
    }
}

function updateSelectedTimeRangeText() {
    const timeRange = document.getElementById('timeRange').value;
    const selectedTimeRangeText = document.getElementById('selectedTimeRange');
    const startTime = document.getElementById('startTime').value;
    const endTime = document.getElementById('endTime').value;

    let text = '';
    const now = new Date();

    switch (timeRange) {
        case 'today':
            text = `时间范围：${formatDate(now)} 00:00 至 ${formatDate(now)} 23:59`;
            break;
        case 'yesterday':
            const yesterday = new Date(now);
            yesterday.setDate(yesterday.getDate() - 1);
            text = `时间范围：${formatDate(yesterday)} 00:00 至 ${formatDate(yesterday)} 23:59`;
            break;
        case 'thisMonth':
            const firstDay = new Date(now.getFullYear(), now.getMonth(), 1);
            text = `时间范围：${formatDate(firstDay)} 00:00 至 ${formatDate(now)} 23:59`;
            break;
        case 'lastMonth':
            const lastMonthStart = new Date(now.getFullYear(), now.getMonth() - 1, 1);
            const lastMonthEnd = new Date(now.getFullYear(), now.getMonth(), 0);
            text = `时间范围：${formatDate(lastMonthStart)} 00:00 至 ${formatDate(lastMonthEnd)} 23:59`;
            break;
        case 'last15Days':
            const fifteenDaysAgo = new Date(now);
            fifteenDaysAgo.setDate(now.getDate() - 15);
            text = `时间范围：${formatDate(fifteenDaysAgo)} 00:00 至 ${formatDate(now)} 23:59`;
            break;
        case 'last30Days':
            const thirtyDaysAgo = new Date(now);
            thirtyDaysAgo.setDate(now.getDate() - 30);
            text = `时间范围：${formatDate(thirtyDaysAgo)} 00:00 至 ${formatDate(now)} 23:59`;
            break;
        case 'custom':
            if (startTime && endTime) {
                text = `时间范围：${formatDateTime(new Date(startTime))} 至 ${formatDateTime(new Date(endTime))}`;
            }
            break;
        case 'all':
            text = '时间范围：全部时间';
            break;
    }

    selectedTimeRangeText.textContent = text;
}

function formatDate(date) {
    const year = date.getFullYear();
    const month = String(date.getMonth() + 1).padStart(2, '0');
    const day = String(date.getDate()).padStart(2, '0');
    return `${year}-${month}-${day}`;
}

function formatDateTime(date) {
    return `${formatDate(date)} ${String(date.getHours()).padStart(2, '0')}:${String(date.getMinutes()).padStart(2, '0')}`;
}

function setupCustomTimeRangeListeners() {
    const startTimeInput = document.getElementById('startTime');
    const endTimeInput = document.getElementById('endTime');
    startTimeInput.addEventListener('change', () => {
        updateStatistics();
        updateSelectedTimeRangeText();
    });
    endTimeInput.addEventListener('change', () => {
        updateStatistics();
        updateSelectedTimeRangeText();
    });
}

function setupStatusFilter() {
    const statusFilter = document.getElementById('statusFilter');
    statusFilter.addEventListener('change', updateCharts);
}

function updateCharts() {
    const buyerId = document.getElementById('buyerSelect').value;
    const status = document.getElementById('statusFilter').value;

    fetch(`/api/get/statistics/daily?buyerId=${buyerId}&status=${status}`)
        .then(response => response.json())
        .then(dailyPayload => {
            if (!dailyPayload.success) {
                throw new Error('日统计加载失败');
            }
            const dailyStats = dailyPayload.data || [];
            fetch(`/api/get/statistics/hourly?buyerId=${buyerId}&status=${status}`)
                .then(response => response.json())
                .then(hourlyPayload => {
                    if (!hourlyPayload.success) {
                        throw new Error('小时统计加载失败');
                    }
                    const hourlyStats = hourlyPayload.data || [];
                    drawCharts(dailyStats, hourlyStats);
                });
        })
        .catch(error => console.error('Error updating charts:', error));
}

function formatDate(dateString) {
    const options = {month: '2-digit', day: '2-digit'};
    return new Date(dateString).toLocaleDateString('zh-CN', options);
}

function generateDateRange(startDate, endDate) {
    const dateArray = [];
    let currentDate = new Date(startDate);
    while (currentDate <= endDate) {
        dateArray.push(new Date(currentDate));
        currentDate.setDate(currentDate.getDate() + 1);
    }
    return dateArray;
}

function fillMissingDates(data, startDate, endDate) {
    const filledData = {};
    data.forEach(item => {
        filledData[item.date] = item;
    });

    const dateArray = generateDateRange(new Date(startDate), new Date(endDate));
    return dateArray.map(date => {
        const formattedDate = date.toISOString().split('T')[0];
        return filledData[formattedDate] || {date: formattedDate, total: 0, earnings: 0};
    });
}

function generateHourRange() {
    return Array.from({length: 24}, (_, i) => i);
}

function fillMissingHours(data) {
    const filledData = {};
    data.forEach(item => {
        filledData[item.hour] = item;
    });

    return generateHourRange().map(hour => {
        return filledData[hour] || {hour: hour, total: 0, earnings: 0};
    });
}

// 图表配置
const chartConfig = {
    type: 'line',
    options: {
        responsive: true,
        maintainAspectRatio: false,
        interaction: {
            mode: 'index',
            intersect: false,
        },
        plugins: {
            legend: {
                position: 'top',
                align: 'center',
                labels: {
                    usePointStyle: true,
                    padding: 20,
                    font: {
                        size: 13
                    }
                }
            },
            tooltip: {
                mode: 'index',
                intersect: false,
                backgroundColor: 'rgba(255, 255, 255, 0.9)',
                titleColor: '#2d3748',
                bodyColor: '#4a5568',
                borderColor: '#e2e8f0',
                borderWidth: 1,
                padding: 12,
                bodyFont: {
                    size: 13
                },
                titleFont: {
                    size: 14,
                    weight: '600'
                },
                boxPadding: 6
            }
        },
        scales: {
            x: {
                grid: {
                    display: false
                },
                ticks: {
                    font: {
                        size: 12
                    }
                }
            },
            y: {
                beginAtZero: true,
                grid: {
                    color: 'rgba(226, 232, 240, 0.5)'
                },
                ticks: {
                    font: {
                        size: 12
                    }
                }
            }
        }
    }
};

// 深色模式配置
const darkModeConfig = {
    ...chartConfig,
    options: {
        ...chartConfig.options,
        plugins: {
            ...chartConfig.options.plugins,
            tooltip: {
                ...chartConfig.options.plugins.tooltip,
                backgroundColor: 'rgba(26, 32, 44, 0.9)',
                titleColor: '#e2e8f0',
                bodyColor: '#a0aec0',
                borderColor: '#2d3748'
            },
            legend: {
                ...chartConfig.options.plugins.legend,
                labels: {
                    ...chartConfig.options.plugins.legend.labels,
                    color: '#e2e8f0'
                }
            }
        },
        scales: {
            x: {
                ...chartConfig.options.scales.x,
                ticks: {
                    ...chartConfig.options.scales.x.ticks,
                    color: '#a0aec0'
                }
            },
            y: {
                ...chartConfig.options.scales.y,
                grid: {
                    color: 'rgba(45, 55, 72, 0.5)'
                },
                ticks: {
                    ...chartConfig.options.scales.y.ticks,
                    color: '#a0aec0'
                }
            }
        }
    }
};

// 根据系统主题选择配置
const getChartConfig = () => {
    return window.matchMedia('(prefers-color-scheme: dark)').matches ? darkModeConfig : chartConfig;
};

// 更新图表时应用配置
function updateDailyStatsChart(data) {
    const config = getChartConfig();
    config.data = {
        labels: data.labels,
        datasets: [
            {
                label: '每日总数',
                data: data.counts,
                borderColor: '#4CAF50',
                backgroundColor: 'rgba(76, 175, 80, 0.1)',
                borderWidth: 2,
                tension: 0.4,
                fill: true,
                pointRadius: 4,
                pointHoverRadius: 6
            },
            {
                label: '每日收益',
                data: data.earnings,
                borderColor: '#3B82F6',
                backgroundColor: 'rgba(59, 130, 246, 0.1)',
                borderWidth: 2,
                tension: 0.4,
                fill: true,
                pointRadius: 4,
                pointHoverRadius: 6
            }
        ]
    };

    if (dailyStatsChart) {
        dailyStatsChart.destroy();
    }
    dailyStatsChart = new Chart(document.getElementById('dailyStatsChart'), config);
}

function updateHourlyStatsChart(data) {
    const config = getChartConfig();
    config.type = 'bar';
    config.data = {
        labels: data.labels,
        datasets: [
            {
                label: '时段分布',
                data: data.counts,
                backgroundColor: 'rgba(76, 175, 80, 0.2)',
                borderColor: '#4CAF50',
                borderWidth: 1,
                borderRadius: 4,
                hoverBackgroundColor: 'rgba(76, 175, 80, 0.3)'
            }
        ]
    };

    if (hourlyStatsChart) {
        hourlyStatsChart.destroy();
    }
    hourlyStatsChart = new Chart(document.getElementById('hourlyStatsChart'), config);
}

function drawCharts(dailyStats, hourlyStats) {
    const dailyCtx = document.getElementById('dailyStatsChart').getContext('2d');
    const hourlyCtx = document.getElementById('hourlyStatsChart').getContext('2d');

    const dailyStartDate = dailyStats.length > 0 ? dailyStats[0].date : new Date();
    const dailyEndDate = dailyStats.length > 0 ? dailyStats[dailyStats.length - 1].date : new Date();

    const filledDailyStats = fillMissingDates(dailyStats, dailyStartDate, dailyEndDate);
    const filledHourlyStats = fillMissingHours(hourlyStats);

    const dailyLabels = filledDailyStats.map(stat => formatDate(stat.date));
    const dailyTotalData = filledDailyStats.map(stat => stat.total);
    const dailyEarningsData = filledDailyStats.map(stat => stat.earnings);

    const hourlyLabels = filledHourlyStats.map(stat => `${stat.hour}:00`);
    const hourlyTotalData = filledHourlyStats.map(stat => stat.total);
    const hourlyEarningsData = filledHourlyStats.map(stat => stat.earnings);

    if (window.dailyChart) {
        window.dailyChart.destroy();
    }

    if (window.hourlyChart) {
        window.hourlyChart.destroy();
    }

    // 检测是否为移动端
    const isMobile = window.innerWidth <= 768;

    const commonOptions = {
        responsive: true,
        maintainAspectRatio: false,
        scales: {
            y: {
                beginAtZero: true,
                ticks: {
                    stepSize: 1,
                    maxTicksLimit: isMobile ? 6 : 8,
                    font: {
                        size: isMobile ? 10 : 12
                    }
                },
                position: 'left',
                title: {
                    display: !isMobile,
                    text: '数目'
                }
            },
            y1: {
                beginAtZero: true,
                ticks: {
                    stepSize: 1,
                    maxTicksLimit: isMobile ? 6 : 8,
                    font: {
                        size: isMobile ? 10 : 12
                    }
                },
                position: 'right',
                grid: {
                    drawOnChartArea: false
                },
                title: {
                    display: !isMobile,
                    text: '金额'
                }
            },
            x: {
                ticks: {
                    maxRotation: isMobile ? 45 : 0,
                    minRotation: isMobile ? 45 : 0,
                    font: {
                        size: isMobile ? 10 : 12
                    }
                }
            }
        },
        plugins: {
            legend: {
                display: true,
                position: isMobile ? 'bottom' : 'top',
                labels: {
                    boxWidth: isMobile ? 12 : 20,
                    padding: isMobile ? 10 : 20,
                    font: {
                        size: isMobile ? 10 : 12
                    }
                }
            },
            tooltip: {
                mode: 'index',
                intersect: false,
                padding: isMobile ? 8 : 12,
                titleFont: {
                    size: isMobile ? 12 : 14
                },
                bodyFont: {
                    size: isMobile ? 11 : 13
                }
            }
        },
        layout: {
            padding: {
                right: isMobile ? 15 : 20
            }
        }
    };

    window.dailyChart = new Chart(dailyCtx, {
        type: 'bar',
        data: {
            labels: dailyLabels,
            datasets: [{
                label: '每日总数',
                data: dailyTotalData,
                backgroundColor: 'rgba(75, 192, 192, 0.5)',
                borderColor: 'rgba(75, 192, 192, 1)',
                borderWidth: 1,
                yAxisID: 'y'
            }, {
                label: '每日收益',
                data: dailyEarningsData,
                type: 'line',
                borderColor: 'rgba(153, 102, 255, 1)',
                borderWidth: isMobile ? 1.5 : 2,
                fill: false,
                tension: 0.1,
                yAxisID: 'y1'
            }]
        },
        options: commonOptions
    });

    window.hourlyChart = new Chart(hourlyCtx, {
        type: 'bar',
        data: {
            labels: hourlyLabels,
            datasets: [{
                label: '每小时总数',
                data: hourlyTotalData,
                backgroundColor: 'rgba(54, 162, 235, 0.5)',
                borderColor: 'rgba(54, 162, 235, 1)',
                borderWidth: 1,
                yAxisID: 'y'
            }, {
                label: '每小时收益',
                data: hourlyEarningsData,
                type: 'line',
                borderColor: 'rgba(255, 159, 64, 1)',
                borderWidth: isMobile ? 1.5 : 2,
                fill: false,
                tension: 0.1,
                yAxisID: 'y1'
            }]
        },
        options: commonOptions
    });
}
