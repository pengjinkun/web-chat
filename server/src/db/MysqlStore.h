#pragma once

#include <nlohmann/json_fwd.hpp>

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

struct UserRow {
  int64_t id{0};
  std::string username;
  std::string nickname;
  std::string password;
};

struct FriendRow {
  int64_t id{0};
  std::string username;
  std::string nickname;
};

struct GroupRow {
  int64_t id{0};
  std::string group_name;
  int64_t owner_id{0};
};

struct ApplyRow {
  int64_t id{0};
  int64_t from_uid{0};
  int status{0};
  std::string create_time;
  std::string username;
  std::string nickname;
};

struct P2pMsgRow {
  int64_t id{0};
  int64_t fromUid{0};
  int64_t toUid{0};
  std::string content;
  int msgType{1};
  std::string createTime;
};

struct GroupMsgRow {
  int64_t id{0};
  int64_t groupId{0};
  int64_t fromUid{0};
  std::string content;
  int msgType{1};
  std::string createTime;
};

class MysqlStore {
 public:
  MysqlStore(std::string host, unsigned port, std::string user, std::string pass, std::string db);
  ~MysqlStore();
  bool connect();

  std::string escape(const std::string& s);

  bool userExistsByUsername(const std::string& username);
  bool insertUser(const std::string& username, const std::string& passMd5, const std::string& nickname);
  bool getUserByUsername(const std::string& username, UserRow& out);
  bool getUserById(int64_t id, UserRow& out);

  bool isFriend(int64_t a, int64_t b);
  bool insertFriendApply(int64_t fromUid, int64_t toUid);
  bool hasPendingApply(int64_t fromUid, int64_t toUid);
  bool getFriendApply(int64_t applyId, int64_t toUid, int64_t& outFrom, int& outStatus);
  bool setFriendApplyStatus(int64_t applyId, int status);
  bool insertFriendRelationIgnore(int64_t userId, int64_t friendId);
  int countPendingApplies(int64_t toUid);
  std::vector<ApplyRow> listFriendApplies(int64_t toUid);
  std::vector<FriendRow> listFriends(int64_t uid);
  std::vector<GroupRow> listGroups(int64_t uid);
  std::vector<FriendRow> searchUsers(const std::string& likePattern, int64_t excludeUid);
  std::vector<GroupRow> searchGroups(const std::string& likePattern);

  int64_t insertGroup(const std::string& name, int64_t ownerId);
  bool insertGroupMember(int64_t gid, int64_t uid);
  bool getGroupById(int64_t gid, GroupRow& out);
  bool isGroupMember(int64_t gid, int64_t uid);
  std::vector<int64_t> listGroupMemberIds(int64_t gid);

  int64_t insertP2pMessage(int64_t fromUid, int64_t toUid, const std::string& content, int msgType);
  int64_t insertGroupMessage(int64_t gid, int64_t fromUid, const std::string& content, int msgType);
  std::vector<P2pMsgRow> historyP2p(int64_t uid, int64_t peer, int limit);
  std::vector<GroupMsgRow> historyGroup(int64_t gid, int limit);
  bool markP2pRead(int64_t uid, int64_t fromUid);

  std::vector<int64_t> listFriendIds(int64_t uid);

 private:
  void* mysql_{nullptr};
  std::string host_;
  unsigned port_{3306};
  std::string user_;
  std::string pass_;
  std::string db_;
  std::mutex mtx_;
};
