(function () {
    if (typeof window === 'undefined' || typeof window.fetch !== 'function') {
        return;
    }

    var LOGIN_PATH = '/login.html';
    var redirecting = false;
    var originalFetch = window.fetch.bind(window);

    function isLoginPage() {
        var path = window.location.pathname || '';
        return path === LOGIN_PATH || path.endsWith('/login.html');
    }

    function redirectToLogin() {
        if (redirecting || isLoginPage()) {
            return;
        }
        redirecting = true;
        window.location.href = LOGIN_PATH;
    }

    function handleAuthFailure() {
        redirectToLogin();
    }

    function cacheUser(user) {
        if (user && typeof user === 'object') {
            window.__currentUser = user;
        }
        return user;
    }

    function fetchCurrentUser() {
        return originalFetch('/api/user', {
            method: 'GET',
            credentials: 'include',
            headers: {
                'Accept': 'application/json'
            }
        }).then(function (response) {
            if (response.status === 401 || response.status === 403) {
                handleAuthFailure();
                return null;
            }
            if (!response.ok) {
                return null;
            }
            return response.json().then(cacheUser).catch(function () {
                return null;
            });
        }).catch(function () {
            handleAuthFailure();
            return null;
        });
    }

    window.fetch = function () {
        return originalFetch.apply(window, arguments).then(function (response) {
            if (response && (response.status === 401 || response.status === 403)) {
                handleAuthFailure();
            }
            return response;
        }).catch(function (error) {
            handleAuthFailure();
            throw error;
        });
    };

    if (!isLoginPage()) {
        if (document.readyState === 'loading') {
            document.addEventListener('DOMContentLoaded', fetchCurrentUser);
        } else {
            fetchCurrentUser();
        }
    }

    window.ensureAuthenticated = function () {
        if (window.__currentUser) {
            return Promise.resolve(window.__currentUser);
        }
        return fetchCurrentUser();
    };
})();
