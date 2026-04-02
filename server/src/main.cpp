#include "chat/ChatHandler.h"
#include "db/MysqlStore.h"
#include "net/EpollServer.h"
#include "redis/RedisStore.h"
#include "Logger.h"

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

static std::atomic<bool> gStop{false};
static EpollServer* gServer = nullptr;

static void onSig(int) {
  gStop = true;
  if (gServer) gServer->stop();
}


int main() {
  std::string mysqlHost = "127.0.0.1";
  unsigned mysqlPort = 3306;
  std::string mysqlUser = "root";
  std::string mysqlPass = "Pjk123456";
  std::string mysqlDb = "chat_db";

  std::string redisHost = "127.0.0.1";
  int redisPort = 6379;
  uint16_t listenPort = 8765;

  MysqlStore db(mysqlHost, mysqlPort, mysqlUser, mysqlPass, mysqlDb);
  if (!db.connect()) {
    std::cerr << "MySQL connect failed. Check MYSQL_* env and schema.sql\n";
    return 1;
  }
  RedisStore redis(redisHost, redisPort);
  if (!redis.connect()) {
    std::cerr << "Redis connect failed. Check REDIS_HOST / REDIS_PORT\n";
    return 1;
  }

  ChatHandler chat(db, redis);
  EpollServer server(chat);
  chat.attachNetwork(&server);
  gServer = &server;

  if (!server.start(listenPort)) {
    std::cerr << "Failed to listen on port " << listenPort << "\n";
    return 1;
  }

  logInfo(("Web Chat C++ (Epoll Boss+Worker) ws://0.0.0.0:" + std::to_string(listenPort)).c_str());

  std::signal(SIGINT, onSig);
  std::signal(SIGTERM, onSig);

  server.join();
  gServer = nullptr;
  return 0;
}
