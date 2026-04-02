#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>
#include <unordered_map>

class EpollServer;
class MysqlStore;
class RedisStore;

class ChatHandler {
 public:
  ChatHandler(MysqlStore& db, RedisStore& redis);
  void attachNetwork(EpollServer* net);

  void onMessage(int fd, const std::string& text);
  void onDisconnect(int fd);

 private:
  void reply(int fd, const nlohmann::json& envelope);
  void sendOrQueueOffline(int64_t uid, const nlohmann::json& envelope);

  MysqlStore& db_;
  RedisStore& redis_;
  EpollServer* net_{nullptr};
  std::unordered_map<int, int64_t> fdToUid_;
};
