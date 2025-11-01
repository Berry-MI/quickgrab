// 获取当前用户信息
async function fetchUser() {
    try {
        const response = await fetch('/api/get/user', {
            method: 'GET',
            headers: {
                'Content-Type': 'application/json'
            }
        });
        if (response.ok) {
            const payload = await response.json();
            if (payload.success && payload.data) {
                const account = payload.data.account || payload.data;
                const displayName = account.displayName || account.username || '未登录';
                document.getElementById('username').textContent = displayName;
            } else {
                console.error('Unexpected user response', payload);
            }
        } else {
            console.error('Failed to fetch user data');
        }
    } catch (error) {
        console.error('Error fetching user data:', error);
    }
}

// 登出功能
document.getElementById('logout-button').addEventListener('click', async () => {
    try {
        const response = await fetch('/api/session/logout', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            }
        });
        if (response.ok) {
            window.location.href = '/login.html';
        } else {
            console.error('Failed to log out');
        }
    } catch (error) {
        console.error('Error logging out:', error);
    }
});

// 页面加载时获取用户信息
document.addEventListener('DOMContentLoaded', fetchUser);
