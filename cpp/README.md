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
- service/：业务逻辑（GrabService、ProxyService、QueryService）。
- workflow/GrabWorkflow：封装抢购状态机、重试与 ReConfirm/CreateOrder 调用。
- proxy/ProxyPool：代理池，提供粘滞分配、失败退避、快照导出。
- repository/：MySqlConnectionPool、RequestsRepository、ResultsRepository 通过 MySQL Connector/C++ X DevAPI 读取/写入表数据。
默认在 cpp/data/database.json 加载数据库连接（如缺失则使用 127.0.0.1:33060/grab_system）；可通过环境变量 QUICKGRAB_DB_HOST/PORT/USER/PASSWORD/NAME/POOL 覆盖。
| RequestsMapper.java | repository/RequestsRepository（基于 MySQL） |
| ResultsMapper.java | repository/ResultsRepository |

5. 若需要多实例协同，可结合分布式锁或消息队列实现任务调度一致性。

## 代理池与抢购流程

- 代理池支持粘滞绑定、成功/失败反馈、隔离与快照；REST /api/proxies、/api/proxies/hydrate 暴露管理接口。
- 抢购流程解析扩展字段（快速模式/稳定模式/自动选点），利用 steady_timer 精准等待后在工作线程池中执行 ReConfirm/CreateOrder，完成后返回到 I/O 线程向调用方响应。

## 与 Java 项目映射

| Java 模块 | C++ 对应实现 |
|-----------|--------------|
| GrabService.java | service/GrabService + workflow/GrabWorkflow + util/HttpClient |
| ProxyController.java | controller/ProxyController |
| QueryController.java | controller/QueryController |
| RequestsMapper.java | epository/RequestsRepository（基于 MySQL） |
| ResultsMapper.java | epository/ResultsRepository |
| NetworkUtil.java | util/HttpClient、workflow/GrabWorkflow、proxy/ProxyPool |
| GrabSchedule.java | GrabWorkflow + oost::asio::steady_timer 调度 |

## 后续建议

1. 补齐图片上传、短链展开等代理透传逻辑，完善 ProxyController。
2. 完成代理 CONNECT/HTTPS 支持与健康检查，增强 HttpClient 的适配能力。
3. 引入配置中心或环境模板（YAML/JSON/TOML）管理多实例部署参数。
4. 编写单元测试和端到端压测脚本，验证抢购流程与代理池弹性。
5. 若需要多实例协同，可结合分布式锁或消息队列实现任务调度一致性。
