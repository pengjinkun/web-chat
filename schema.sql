-- chat_db — 与设计文档一致，供 MySQL 部署使用
CREATE DATABASE IF NOT EXISTS chat_db DEFAULT CHARSET utf8mb4;
USE chat_db;

CREATE TABLE IF NOT EXISTS `user` (
  `id` BIGINT PRIMARY KEY AUTO_INCREMENT COMMENT '用户ID',
  `username` VARCHAR(32) NOT NULL UNIQUE COMMENT '用户名',
  `password` VARCHAR(64) NOT NULL COMMENT '密码（MD5加密）',
  `nickname` VARCHAR(32) DEFAULT '' COMMENT '昵称',
  `create_time` DATETIME DEFAULT CURRENT_TIMESTAMP,
  `update_time` DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  INDEX idx_username (username)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `friend_relation` (
  `id` BIGINT PRIMARY KEY AUTO_INCREMENT,
  `user_id` BIGINT NOT NULL COMMENT '用户ID',
  `friend_id` BIGINT NOT NULL COMMENT '好友ID',
  `status` TINYINT NOT NULL DEFAULT 1 COMMENT '1-正常 2-删除',
  `create_time` DATETIME DEFAULT CURRENT_TIMESTAMP,
  UNIQUE KEY uk_user_friend (user_id, friend_id),
  INDEX idx_user_id (user_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `friend_apply` (
  `id` BIGINT PRIMARY KEY AUTO_INCREMENT,
  `from_uid` BIGINT NOT NULL COMMENT '申请人ID',
  `to_uid` BIGINT NOT NULL COMMENT '被申请人ID',
  `status` TINYINT DEFAULT 0 COMMENT '0-待处理 1-同意 2-拒绝',
  `create_time` DATETIME DEFAULT CURRENT_TIMESTAMP,
  INDEX idx_to_uid (to_uid)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `group` (
  `id` BIGINT PRIMARY KEY AUTO_INCREMENT COMMENT '群ID',
  `group_name` VARCHAR(64) NOT NULL COMMENT '群名称',
  `owner_id` BIGINT NOT NULL COMMENT '群主ID',
  `create_time` DATETIME DEFAULT CURRENT_TIMESTAMP,
  INDEX idx_group_name (group_name)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `group_member` (
  `id` BIGINT PRIMARY KEY AUTO_INCREMENT,
  `group_id` BIGINT NOT NULL,
  `user_id` BIGINT NOT NULL,
  `create_time` DATETIME DEFAULT CURRENT_TIMESTAMP,
  UNIQUE KEY uk_group_user (group_id, user_id),
  INDEX idx_user_id (user_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `p2p_message` (
  `id` BIGINT PRIMARY KEY AUTO_INCREMENT,
  `from_uid` BIGINT NOT NULL,
  `to_uid` BIGINT NOT NULL,
  `content` TEXT NOT NULL COMMENT '消息内容',
  `msg_type` TINYINT DEFAULT 1 COMMENT '1-普通消息 2-系统事件',
  `is_read` TINYINT DEFAULT 0 COMMENT '0-未读 1-已读',
  `create_time` DATETIME DEFAULT CURRENT_TIMESTAMP,
  INDEX idx_from_to (from_uid, to_uid),
  INDEX idx_to_uid (to_uid)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `group_message` (
  `id` BIGINT PRIMARY KEY AUTO_INCREMENT,
  `group_id` BIGINT NOT NULL,
  `from_uid` BIGINT NOT NULL,
  `content` TEXT NOT NULL,
  `msg_type` TINYINT DEFAULT 1 COMMENT '1-普通消息 2-系统事件',
  `create_time` DATETIME DEFAULT CURRENT_TIMESTAMP,
  INDEX idx_group_id (group_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
