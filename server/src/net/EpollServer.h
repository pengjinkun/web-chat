#pragma once

#include <nlohmann/json.hpp>

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

class ChatHandler;

class EpollServer {
 public:
  explicit EpollServer(ChatHandler& chat);
  ~EpollServer();

  bool start(uint16_t port);
  void stop();
  void join();

  void registerUserConnection(int64_t uid, int fd);
  void unregisterUserConnection(int64_t uid, int fd);
  bool userHasConnection(int64_t uid);

  bool sendWsJson(int fd, const nlohmann::json& envelope);
  void sendWsJsonToUser(int64_t uid, const nlohmann::json& envelope);

 private:
  struct ClientCtx;
  void bossLoop();
  void workerLoop();
  void closeClient(int fd);
  void flushOut(int fd);
  void enqueueRaw(int fd, const std::string& data);

  ChatHandler& chat_;

  int listenFd_{-1};
  int epfd_{-1};
  int wakeFd_{-1};

  std::atomic<bool> running_{false};
  std::thread bossThread_;
  std::thread workerThread_;

  std::mutex pendingMtx_;
  std::deque<int> pendingFds_;

  std::mutex connMtx_;
  std::unordered_map<int, std::unique_ptr<ClientCtx>> conns_;

  std::mutex userMtx_;
  std::unordered_map<int64_t, std::vector<int>> uidToFds_;
};
