#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

class RedisStore {
 public:
  RedisStore(std::string host, int port);
  ~RedisStore();
  bool connect();

  void userOnline(int64_t uid, int64_t connectTimeMs);
  void userOffline(int64_t uid);

  void applyBellClear(int64_t uid);
  void applyBellResetForTarget(int64_t uid);
  bool applyBellShouldShow(int64_t uid, int pendingMysqlCount);

  void incrUnreadP2p(int64_t uid, int64_t fromUid);
  void clearUnreadP2p(int64_t uid, int64_t peerFid);
  void incrUnreadGroup(int64_t uid, int64_t gid);
  void clearUnreadGroup(int64_t uid, int64_t gid);
  nlohmann::json unreadSnapshot(int64_t uid);

  void offlinePush(int64_t uid, const std::string& envelopeJson);
  using OfflineSender = std::function<void(const std::string& wsPayloadJson)>;
  int offlineDrainAndSend(int64_t uid, const OfflineSender& send);

  /** (uid, connectTimeMs) ascending by connect time → longest online first */
  std::vector<std::pair<int64_t, int64_t>> recommendByOnlineOrder(size_t maxMembers);

 private:
  void* redis_{nullptr};
  std::string host_;
  int port_{6379};
  std::mutex mtx_;
};
