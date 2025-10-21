# QuickGrab C++（Boost.Asio/Beast）重构

该目录提供基于 Boost.Asio + Boost.Beast 的 QuickGrab 异步重写，依赖通过 vcpkg 管理。

## 构建方式

`powershell
cd cpp
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake
cmake --build build
`

需要提前安装的 vcpkg 包：

- boost-beast
- boost-json
- boost-system
- openssl
- mysql-connector-cpp (X DevAPI)

## 工程结构

- src/main.cpp：启动入口，负责装配 HTTP 服务器、MySQL X DevAPI 连接池、仓储与业务服务。
- server/：基于 Beast 的 HTTP Server、Router、RequestContext，替代 Spring MVC。
- controller/：REST 接口层（抢购、代理、查询）。
- service/：业务逻辑（GrabService、QueryService、StatisticsService）。
- workflow/GrabWorkflow：封装抢购状态机、重试与 ReConfirm/CreateOrder 调用。
- proxy/ProxyPool：代理池，提供粘滞分配、失败退避、快照导出。
- repository/：MySqlConnectionPool、RequestsRepository、ResultsRepository 通过 MySQL Connector/C++ X DevAPI 读取/写入表数据。
默认在 cpp/data/database.json 加载数据库连接（如缺失则使用 127.0.0.1:33060/grab_system）；可通过环境变量 QUICKGRAB_DB_HOST/PORT/USER/PASSWORD/NAME/POOL 覆盖。

可选在 cpp/data/kdlproxy.json 配置快代理（Kuaidaili）拉取参数：secretId/signature/username/password/count/refreshMinutes，或通过环境变量 QUICKGRAB_PROXY_ENDPOINT/SECRET_ID/SIGNATURE/USERNAME/PASSWORD/BATCH/REFRESH_MINUTES 覆盖。启用后服务在抢购请求启用代理时即时调用 `https://dps.kdlapi.com/api/getdps/` 拉取候选 IP，测量延迟后自动挑选最快节点复用。

### HTTPS 信任链配置

- `cpp/data/cacert.pem` 提供与 curl 同源的 CA 证书集合，HttpClient 会在启动时优先加载该文件，用于校验代理隧道上的 HTTPS 目标站证书。
  如需更新，可从 <https://curl.se/docs/caextract.html> 下载最新的 `cacert.pem` 覆盖。
- 可通过环境变量 `QUICKGRAB_HOME` 指向包含 `data/cacert.pem` 的 QuickGrab 目录，或使用 `QUICKGRAB_CACERT`、`CURL_CA_BUNDLE`、
  `SSL_CERT_FILE` 显式指定证书路径。若系统目录和上述路径都无法加载，将直接抛出异常阻止在不受信任的环境下继续发起 HTTPS 请求。
| RequestsMapper.java | repository/RequestsRepository（基于 MySQL） |
| ResultsMapper.java | repository/ResultsRepository |

5. 若需要多实例协同，可结合分布式锁或消息队列实现任务调度一致性。

## Nginx 鉴权静态资源示例

前端页面通常通过 Nginx 暴露（如 `http://localhost:90/index.html`）。为了让静态页面与 Java 服务保持一致的登录校验，可以让 Nginx 在回源静态文件前调用后端新增的 `GET /internal/auth/check` 探针：

```nginx
location / {
    auth_request /auth/check;
    error_page 401 = @login;
    root   html/static;
    index  index.html index.htm;
}

location = /auth/check {
    internal;
    proxy_pass http://127.0.0.1:8080/internal/auth/check;
    proxy_pass_request_body off;
    proxy_set_header Content-Length "";
    proxy_set_header X-Original-URI $request_uri;
    proxy_set_header Cookie $http_cookie;
}

location @login {
    return 302 /login.html;
}

location = /login.html {
    root html/static;
}

# 如果还通过 Nginx 代理后端接口（例如 `location ~ ^/(api|checkLatency|fetchItemInfo)`），同样需要把 Cookie 透传给后端：
#
# ```nginx
# proxy_set_header Cookie $http_cookie;
# ```
#
# 否则用户虽然在登录接口拿到了会话 Cookie，但后续接口校验与 `/internal/auth/check` 探针都读不到该 Cookie，会一直重定向回登录页。
```

上述配置会在用户未登录或会话过期时返回 302，引导浏览器重新跳转到登录页；后端同样提供 `GET /api/session` 以供前端轮询当前登录状态。

## 代理池与抢购流程

- 代理池支持粘滞绑定、成功/失败反馈、隔离与快照，可结合 KDL 接口按需自动补充代理。
- 抢购流程解析扩展字段（快速模式/稳定模式/自动选点），利用 steady_timer 精准等待后在工作线程池中执行 ReConfirm/CreateOrder，完成后返回到 I/O 线程向调用方响应。

## 与 Java 项目映射

| Java 模块 | C++ 对应实现 |
|-----------|--------------|
| GrabService.java | service/GrabService + workflow/GrabWorkflow + util/HttpClient |
| ProxyController.java | controller/ProxyController |
| QueryController.java | controller/QueryController |
| RequestsMapper.java | 
epository/RequestsRepository（基于 MySQL） |
| ResultsMapper.java | 
epository/ResultsRepository |
| NetworkUtil.java | util/HttpClient、workflow/GrabWorkflow、proxy/ProxyPool |
| GrabSchedule.java | GrabWorkflow + oost::asio::steady_timer 调度 |

## 后续建议

1. 补齐图片上传、短链展开等代理透传逻辑，完善 ProxyController。
2. 完成代理 CONNECT/HTTPS 支持与健康检查，增强 HttpClient 的适配能力。
3. 引入配置中心或环境模板（YAML/JSON/TOML）管理多实例部署参数。
4. 编写单元测试和端到端压测脚本，验证抢购流程与代理池弹性。
5. 若需要多实例协同，可结合分布式锁或消息队列实现任务调度一致性。
