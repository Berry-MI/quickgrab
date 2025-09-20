-- 删除数据库
DROP DATABASE IF EXISTS grab_system;

-- 创建数据库
CREATE DATABASE IF NOT EXISTS grab_system CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

-- 使用数据库
USE grab_system;

-- 创建抢购者表
CREATE TABLE IF NOT EXISTS buyers
(
    id                     INT AUTO_INCREMENT PRIMARY KEY COMMENT '主键，自动递增',
    username               VARCHAR(255) NOT NULL COMMENT '用户名',
    password               VARCHAR(255) NOT NULL COMMENT '密码',
    email                  VARCHAR(255) COMMENT '邮箱',
    access_level           INT COMMENT '权限级别',
    daily_max_submissions  INT COMMENT '日最大提交数量',
    daily_submission_count INT DEFAULT 0 COMMENT '当日提交数量',
    validity_period        DATE COMMENT '有效期',
    UNIQUE (username)
) COMMENT ='存储抢购者信息的表';

-- 创建设备表
CREATE TABLE IF NOT EXISTS devices
(
    id             INT AUTO_INCREMENT PRIMARY KEY COMMENT '主键，自动递增',
    ip_address     VARCHAR(255) NOT NULL COMMENT 'IP地址',
    max_concurrent INT COMMENT '最大并发量',
    priority       INT COMMENT '优先级',
    info           TEXT COMMENT '信息',
    UNIQUE (ip_address)
) COMMENT ='存储设备信息的表';

-- 创建请求表
CREATE TABLE IF NOT EXISTS requests
(
    id                 INT AUTO_INCREMENT PRIMARY KEY COMMENT '主键，自动递增',
    device_id          INT NOT NULL COMMENT '设备ID',
    buyer_id           INT NOT NULL COMMENT '抢购者ID',
    thread_id          VARCHAR(50) COMMENT '线程ID',
    link               TEXT COMMENT '链接',
    cookies            TEXT COMMENT 'Cookies',
    order_info         TEXT COMMENT '订单信息',
    user_info          TEXT COMMENT '用户信息',
    order_template     TEXT COMMENT '下单模板',
    message            TEXT COMMENT '留言',
    id_number          VARCHAR(50) COMMENT '身份证号',
    keyword            TEXT COMMENT '商品关键词',
    start_time         DATETIME COMMENT '开售时间',
    end_time           DATETIME COMMENT '结束时间',
    quantity           INT COMMENT '数量',
    delay              INT COMMENT '延迟（毫秒）',
    frequency          INT COMMENT '频率',
    type               INT COMMENT '类型',
    status             INT COMMENT '状态',
    order_parameters   TEXT COMMENT '下单参数',
    actual_earnings    DECIMAL(10, 2) COMMENT '实际收益',
    estimated_earnings DECIMAL(10, 2) COMMENT '预估收益',
    extension          TEXT COMMENT '扩展',
    FOREIGN KEY (device_id) REFERENCES devices (id),
    FOREIGN KEY (buyer_id) REFERENCES buyers (id)
) COMMENT ='存储抢购请求的表';

-- 创建结果表
CREATE TABLE IF NOT EXISTS results
(
    id                 INT AUTO_INCREMENT PRIMARY KEY COMMENT '主键，自动递增',
    device_id          INT NOT NULL COMMENT '设备ID',
    buyer_id           INT NOT NULL COMMENT '抢购者ID',
    thread_id          VARCHAR(50) COMMENT '线程ID',
    link               TEXT COMMENT '链接',
    cookies            TEXT COMMENT 'Cookies',
    order_info         TEXT COMMENT '订单信息',
    user_info          TEXT COMMENT '用户信息',
    order_template     TEXT COMMENT '下单模板',
    message            TEXT COMMENT '留言',
    id_number          VARCHAR(50) COMMENT '身份证号',
    keyword            TEXT COMMENT '商品关键词',
    start_time         DATETIME COMMENT '开售时间',
    end_time           DATETIME COMMENT '结束时间',
    quantity           INT COMMENT '数量',
    delay              INT COMMENT '延迟（毫秒）',
    frequency          INT COMMENT '频率',
    type               INT COMMENT '类型',
    status             INT COMMENT '状态',
    response_message   TEXT COMMENT '返回信息',
    actual_earnings    DECIMAL(10, 2) COMMENT '实际收益',
    estimated_earnings DECIMAL(10, 2) COMMENT '预估收益',
    extension          TEXT COMMENT '扩展',
    FOREIGN KEY (device_id) REFERENCES devices (id),
    FOREIGN KEY (buyer_id) REFERENCES buyers (id)
) COMMENT ='存储抢购结果的表';

CREATE TABLE admin_config
(
    id           BIGINT AUTO_INCREMENT PRIMARY KEY,                              -- 自增主键
    config_key   VARCHAR(255) NOT NULL UNIQUE,                                   -- 配置项的唯一键
    config_value TEXT         NOT NULL,                                          -- 配置项的值
    created_at   TIMESTAMP DEFAULT CURRENT_TIMESTAMP,                            -- 创建时间，默认为当前时间
    updated_at   TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP -- 更新时间，更新时自动更改为当前时间
);
INSERT INTO admin_config (config_key, config_value)
VALUES ('site_name', 'My Admin Panel'), -- 网站名称
       ('max_users', '100'),            -- 最大用户数
       ('maintenance_mode', 'false'); -- 维护模式（false表示不维护）


CREATE TABLE seq_0_to_14
(
    seq INT PRIMARY KEY
);

INSERT INTO seq_0_to_14 (seq)
VALUES (0),
       (1),
       (2),
       (3),
       (4),
       (5),
       (6),
       (7),
       (8),
       (9),
       (10),
       (11),
       (12),
       (13),
       (14);

# -- 插入抢购者表的示例数据
# INSERT INTO buyers (username, password, email, access_level, daily_max_submissions, validity_period)
# VALUES ('admin', 'admin', '1966099953@qq.com', 5, 1000, '2999-05-01');

INSERT INTO buyers (username, password, email, access_level, daily_max_submissions, validity_period)
VALUES ('admin', 'admin', '1074815569@qq.com', 5, 1000, '2999-05-01');

INSERT INTO buyers (username, password, email, access_level, daily_max_submissions, validity_period)
VALUES ('qwe', 'admin', '1074815569@qq.com', 5, 1000, '2999-05-01');



-- 插入设备表的示例数据
INSERT INTO devices (ip_address, max_concurrent, priority, info)
VALUES ('localhost', 100, 1, '本地设备，测试用');