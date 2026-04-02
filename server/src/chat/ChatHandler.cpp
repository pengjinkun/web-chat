#include "ChatHandler.h"
#include "../db/MysqlStore.h"
#include "../net/EpollServer.h"
#include "../redis/RedisStore.h"
#include "../util/CryptoUtil.h"
#include "../Logger.h"

#include <chrono>
#include <cctype>
#include <ctime>
#include <exception>
#include <sstream>
#include <unordered_map>

namespace {

int64_t nowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

nlohmann::json envOk(const std::string& type, nlohmann::json data = {}) {
  return {{"type", type}, {"data", std::move(data)}, {"timestamp", nowMs()}};
}

nlohmann::json envErr(const std::string& type, const std::string& err) {
  return {{"type", type},
          {"data", nlohmann::json::object()},
          {"timestamp", nowMs()},
          {"error", err}};
}

bool validUsername(const std::string& u) {
  if (u.size() < 3 || u.size() > 16) return false;
  for (char c : u)
    if (!std::isalnum(static_cast<unsigned char>(c))) return false;
  return true;
}

std::string stripPct(const std::string& s) {
  std::string o;
  o.reserve(s.size());
  for (char c : s)
    if (c != '%') o += c;
  return o;
}

}  // namespace

ChatHandler::ChatHandler(MysqlStore& db, RedisStore& redis) : db_(db), redis_(redis) {}

void ChatHandler::attachNetwork(EpollServer* net) { net_ = net; }

void ChatHandler::reply(int fd, const nlohmann::json& envelope) {
  if (net_) net_->sendWsJson(fd, envelope);
}

void ChatHandler::sendOrQueueOffline(int64_t uid, const nlohmann::json& envelope) {
  if (net_ && net_->userHasConnection(uid)) net_->sendWsJsonToUser(uid, envelope);
  else redis_.offlinePush(uid, envelope.dump());
}

void ChatHandler::onDisconnect(int fd) {
  auto it = fdToUid_.find(fd);
  if (it == fdToUid_.end()) return;
  int64_t uid = it->second;
  fdToUid_.erase(it);
  if (net_) net_->unregisterUserConnection(uid, fd);
  if (net_ && !net_->userHasConnection(uid)) redis_.userOffline(uid);
}

void ChatHandler::onMessage(int fd, const std::string& text) {
  nlohmann::json j;
  try {
    j = nlohmann::json::parse(text);
  } catch (...) {
    reply(fd, envErr("error", "invalid json"));
    return;
  }
  std::string type = j.value("type", "");
  nlohmann::json data = nlohmann::json::object();
  if (j.contains("data") && !j["data"].is_null()) {
    if (j["data"].is_object()) data = j["data"];
  }

  try {
    if (type == "user.register") {
      std::string username = data.value("username", "");
      std::string password = data.value("password", "");
      std::string nickname = data.value("nickname", "");
      if (username.empty() || password.empty()) {
        reply(fd, envErr("user.register", "用户名或密码为空"));
        return;
      }
      if (!validUsername(username)) {
        reply(fd, envErr("user.register", "用户名须为3~16位字母或数字"));
        return;
      }
      if (password.size() < 6 || password.size() > 16) {
        reply(fd, envErr("user.register", "密码长度6~16位"));
        return;
      }
      if (db_.userExistsByUsername(username)) {
        reply(fd, envErr("user.register", "用户名已存在"));
        return;
      }
      if (nickname.empty()) nickname = username;
      if (!db_.insertUser(username, md5Hex(password), nickname)) {
        reply(fd, envErr("user.register", "数据库错误"));
        return;
      }
      UserRow row;
      if (!db_.getUserByUsername(username, row)) {
        reply(fd, envErr("user.register", "数据库错误"));
        return;
      }
      reply(fd, envOk("user.register", {{"user", {{"id", row.id}, {"username", row.username}, {"nickname", row.nickname}}}}));
      return;
    }

    if (type == "user.login") {
      std::string username = data.value("username", "");
      std::string password = data.value("password", "");
      UserRow row;
      if (!db_.getUserByUsername(username, row) || row.password != md5Hex(password)) {
        reply(fd, envErr("user.login", "用户名或密码错误"));
        return;
      }
      std::string token = randomHex(24);
      fdToUid_[fd] = row.id;
      if (net_) net_->registerUserConnection(row.id, fd);
      redis_.userOnline(row.id, nowMs());
      auto friends = db_.listFriends(row.id);
      auto groups = db_.listGroups(row.id);
      nlohmann::json unread = redis_.unreadSnapshot(row.id);
      int pending = db_.countPendingApplies(row.id);
      bool bell = redis_.applyBellShouldShow(row.id, pending);
      nlohmann::json fr = nlohmann::json::array();
      for (const auto& f : friends) fr.push_back({{"id", f.id}, {"username", f.username}, {"nickname", f.nickname}});
      nlohmann::json gr = nlohmann::json::array();
      for (const auto& g : groups)
        gr.push_back({{"id", g.id}, {"group_name", g.group_name}, {"owner_id", g.owner_id}});
      reply(fd, envOk("user.login", {{"token", token},
                                     {"user", {{"id", row.id}, {"username", row.username}, {"nickname", row.nickname}}},
                                     {"friends", fr},
                                     {"groups", gr},
                                     {"unread", unread},
                                     {"applyBell", bell},
                                     {"pendingApplyCount", pending}}));
      return;
    }

    auto uit = fdToUid_.find(fd);
    if (uit == fdToUid_.end()) {
      reply(fd, envErr("error", "请先登录"));
      return;
    }
    int64_t uid = uit->second;

    if (type == "friend.search") {
      std::string kw = stripPct(data.value("keyword", ""));
      if (kw.empty()) {
        reply(fd, envOk("friend.search", {{"users", nlohmann::json::array()}}));
        return;
      }
      auto users = db_.searchUsers(kw, uid);
      nlohmann::json arr = nlohmann::json::array();
      for (const auto& u : users) arr.push_back({{"id", u.id}, {"username", u.username}, {"nickname", u.nickname}});
      reply(fd, envOk("friend.search", {{"users", arr}}));
      return;
    }

    if (type == "friend.add") {
      int64_t toId = data.value("toUid", 0);
      if (toId <= 0 || toId == uid) {
        reply(fd, envErr("friend.add", "参数错误"));
        return;
      }
      if (db_.isFriend(uid, toId)) {
        reply(fd, envErr("friend.add", "已是好友"));
        return;
      }
      if (db_.hasPendingApply(uid, toId)) {
        reply(fd, envErr("friend.add", "已发送申请"));
        return;
      }
      if (!db_.insertFriendApply(uid, toId)) {
        reply(fd, envErr("friend.add", "数据库错误"));
        return;
      }
      redis_.applyBellResetForTarget(toId);
      UserRow fromUser;
      db_.getUserById(uid, fromUser);
      nlohmann::json env =
          envOk("notify.friend_apply", {{"from", {{"id", fromUser.id}, {"username", fromUser.username}, {"nickname", fromUser.nickname}}}});
      sendOrQueueOffline(toId, env);
      reply(fd, envOk("friend.add", {{"ok", true}}));
      return;
    }

    if (type == "friend.apply.list") {
      auto rows = db_.listFriendApplies(uid);
      nlohmann::json arr = nlohmann::json::array();
      for (const auto& r : rows) {
        arr.push_back({{"id", r.id},
                       {"from_uid", r.from_uid},
                       {"status", r.status},
                       {"create_time", r.create_time},
                       {"username", r.username},
                       {"nickname", r.nickname}});
      }
      reply(fd, envOk("friend.apply.list", {{"list", arr}}));
      return;
    }

    if (type == "friend.apply.read") {
      redis_.applyBellClear(uid);
      reply(fd, envOk("friend.apply.read", {{"ok", true}}));
      return;
    }

    if (type == "friend.agree") {
      int64_t applyId = data.value("applyId", 0);
      int64_t fromUid = 0;
      int st = -1;
      if (!db_.getFriendApply(applyId, uid, fromUid, st) || st != 0) {
        reply(fd, envErr("friend.agree", "申请不存在或已处理"));
        return;
      }
      db_.setFriendApplyStatus(applyId, 1);
      db_.insertFriendRelationIgnore(uid, fromUid);
      db_.insertFriendRelationIgnore(fromUid, uid);
      UserRow u1, u2;
      db_.getUserById(uid, u1);
      db_.getUserById(fromUid, u2);
      std::string sys1 = "你和 " + u2.username + " 已成为好友，可以开始聊天啦";
      std::string sys2 = "你和 " + u1.username + " 已成为好友，可以开始聊天啦";
      int64_t id1 = db_.insertP2pMessage(uid, fromUid, sys1, 2);
      int64_t id2 = db_.insertP2pMessage(fromUid, uid, sys2, 2);
      char isoBuf[64];
      std::time_t tt = std::time(nullptr);
      std::strftime(isoBuf, sizeof isoBuf, "%Y-%m-%dT%H:%M:%S", std::gmtime(&tt));
      std::string iso = std::string(isoBuf) + ".000Z";
      nlohmann::json d1 = {{"id", id1}, {"fromUid", uid}, {"toUid", fromUid}, {"content", sys1}, {"msgType", 2}, {"createTime", iso}};
      nlohmann::json d2 = {{"id", id2}, {"fromUid", fromUid}, {"toUid", uid}, {"content", sys2}, {"msgType", 2}, {"createTime", iso}};
      if (net_ && net_->userHasConnection(uid)) net_->sendWsJsonToUser(uid, envOk("chat.p2p", d1));
      else redis_.offlinePush(uid, envOk("chat.p2p", d1).dump());
      sendOrQueueOffline(fromUid, envOk("chat.p2p", d2));
      if (!net_ || !net_->userHasConnection(fromUid)) redis_.incrUnreadP2p(fromUid, uid);
      reply(fd, envOk("friend.agree", {{"ok", true}, {"friend", {{"id", u2.id}, {"username", u2.username}, {"nickname", u2.nickname}}}}));
      if (net_) net_->sendWsJsonToUser(fromUid, envOk("friend.agree", {{"ok", true}, {"friend", {{"id", u1.id}, {"username", u1.username}, {"nickname", u1.nickname}}}}));
      return;
    }

    if (type == "friend.apply.reject") {
      int64_t applyId = data.value("applyId", 0);
      int64_t fromUid = 0;
      int st = -1;
      if (!db_.getFriendApply(applyId, uid, fromUid, st) || st != 0) {
        reply(fd, envErr("friend.apply.reject", "申请不存在或已处理"));
        return;
      }
      db_.setFriendApplyStatus(applyId, 2);
      reply(fd, envOk("friend.apply.reject", {{"ok", true}}));
      return;
    }

    if (type == "friend.recommend") {
      auto order = redis_.recommendByOnlineOrder(200);
      auto friendIds = db_.listFriendIds(uid);
      std::unordered_map<int64_t, char> fset;
      for (int64_t f : friendIds) fset[f] = 1;
      nlohmann::json arr = nlohmann::json::array();
      int64_t now = nowMs();
      for (const auto& pr : order) {
        int64_t ouid = pr.first;
        int64_t connectMs = pr.second;
        if (ouid == uid || fset.count(ouid)) continue;
        UserRow ur;
        if (!db_.getUserById(ouid, ur)) continue;
        int64_t sec = (now - connectMs) / 1000;
        if (sec < 0) sec = 0;
        arr.push_back(
            {{"id", ur.id}, {"username", ur.username}, {"nickname", ur.nickname}, {"onlineSeconds", sec}});
        if (arr.size() >= 20) break;
      }
      reply(fd, envOk("friend.recommend", {{"users", arr}}));
      return;
    }

    if (type == "group.create") {
      std::string name = stripPct(data.value("groupName", ""));
      if (name.empty()) {
        reply(fd, envErr("group.create", "群名不能为空"));
        return;
      }
      int64_t gid = db_.insertGroup(name, uid);
      if (!gid) {
        reply(fd, envErr("group.create", "数据库错误"));
        return;
      }
      db_.insertGroupMember(gid, uid);
      std::string sys = "你创建了群聊【" + name + "】";
      int64_t mid = db_.insertGroupMessage(gid, uid, sys, 2);
      char isoBuf[64];
      std::time_t tt = std::time(nullptr);
      std::strftime(isoBuf, sizeof isoBuf, "%Y-%m-%dT%H:%M:%S", std::gmtime(&tt));
      std::string iso = std::string(isoBuf) + ".000Z";
      reply(fd, envOk("group.create", {{"group", {{"id", gid}, {"group_name", name}, {"owner_id", uid}}},
                                        {"systemMessage",
                                         {{"id", mid},
                                          {"groupId", gid},
                                          {"fromUid", uid},
                                          {"content", sys},
                                          {"msgType", 2},
                                          {"createTime", iso}}}}));
      return;
    }

    if (type == "group.join") {
      int64_t gid = data.value("groupId", 0);
      GroupRow g;
      if (!db_.getGroupById(gid, g)) {
        reply(fd, envErr("group.join", "群不存在"));
        return;
      }
      if (db_.isGroupMember(gid, uid)) {
        reply(fd, envErr("group.join", "已在群内"));
        return;
      }
      db_.insertGroupMember(gid, uid);
      UserRow u;
      db_.getUserById(uid, u);
      std::string sys = u.username + " 加入了群聊";
      int64_t mid = db_.insertGroupMessage(gid, uid, sys, 2);
      char isoBuf[64];
      std::time_t tt = std::time(nullptr);
      std::strftime(isoBuf, sizeof isoBuf, "%Y-%m-%dT%H:%M:%S", std::gmtime(&tt));
      std::string iso = std::string(isoBuf) + ".000Z";
      nlohmann::json body = {{"id", mid}, {"groupId", gid}, {"fromUid", uid}, {"content", sys}, {"msgType", 2}, {"createTime", iso}};
      reply(fd, envOk("chat.group", body));
      auto members = db_.listGroupMemberIds(gid);
      for (int64_t m : members) {
        if (m == uid) continue;
        nlohmann::json env = envOk("chat.group", body);
        if (net_ && net_->userHasConnection(m)) net_->sendWsJsonToUser(m, env);
        else {
          redis_.incrUnreadGroup(m, gid);
          redis_.offlinePush(m, env.dump());
        }
      }
      reply(fd, envOk("group.join", {{"group", {{"id", g.id}, {"group_name", g.group_name}}}, {"ok", true}}));
      return;
    }

    if (type == "group.search") {
      std::string kw = stripPct(data.value("keyword", ""));
      if (kw.empty()) {
        reply(fd, envOk("group.search", {{"groups", nlohmann::json::array()}}));
        return;
      }
      auto groups = db_.searchGroups(kw);
      nlohmann::json arr = nlohmann::json::array();
      for (const auto& g : groups)
        arr.push_back({{"id", g.id}, {"group_name", g.group_name}, {"owner_id", g.owner_id}});
      reply(fd, envOk("group.search", {{"groups", arr}}));
      return;
    }

    if (type == "chat.p2p") {
      int64_t toUid = data.value("toUid", 0);
      std::string content = data.value("content", "");
      if (toUid <= 0 || content.empty()) {
        reply(fd, envErr("chat.p2p", "消息无效"));
        return;
      }
      if (!db_.isFriend(uid, toUid)) {
        reply(fd, envErr("chat.p2p", "非好友"));
        return;
      }
      int64_t id = db_.insertP2pMessage(uid, toUid, content, 1);
      char isoBuf[64];
      std::time_t tt = std::time(nullptr);
      std::strftime(isoBuf, sizeof isoBuf, "%Y-%m-%dT%H:%M:%S", std::gmtime(&tt));
      std::string iso = std::string(isoBuf) + ".000Z";
      nlohmann::json body = {{"id", id}, {"fromUid", uid}, {"toUid", toUid}, {"content", content}, {"msgType", 1}, {"createTime", iso}};
      reply(fd, envOk("chat.p2p", body));
      nlohmann::json env = envOk("chat.p2p", body);
      if (net_ && net_->userHasConnection(toUid)) net_->sendWsJsonToUser(toUid, env);
      else {
        redis_.incrUnreadP2p(toUid, uid);
        redis_.offlinePush(toUid, env.dump());
      }
      return;
    }

    if (type == "chat.group") {
      int64_t gid = data.value("groupId", 0);
      std::string content = data.value("content", "");
      if (gid <= 0 || content.empty()) {
        reply(fd, envErr("chat.group", "消息无效"));
        return;
      }
      if (!db_.isGroupMember(gid, uid)) {
        reply(fd, envErr("chat.group", "不在群内"));
        return;
      }
      int64_t id = db_.insertGroupMessage(gid, uid, content, 1);
      char isoBuf[64];
      std::time_t tt = std::time(nullptr);
      std::strftime(isoBuf, sizeof isoBuf, "%Y-%m-%dT%H:%M:%S", std::gmtime(&tt));
      std::string iso = std::string(isoBuf) + ".000Z";
      nlohmann::json body = {{"id", id}, {"groupId", gid}, {"fromUid", uid}, {"content", content}, {"msgType", 1}, {"createTime", iso}};
      auto members = db_.listGroupMemberIds(gid);
      for (int64_t m : members) {
        nlohmann::json env = envOk("chat.group", body);
        if (m == uid) {
          reply(fd, env);
        } else if (net_ && net_->userHasConnection(m)) {
          net_->sendWsJsonToUser(m, env);
        } else {
          redis_.incrUnreadGroup(m, gid);
          redis_.offlinePush(m, env.dump());
        }
      }
      return;
    }

    if (type == "msg.history") {
      std::string peerType = data.value("peerType", "");
      int64_t peerId = data.value("peerId", 0);
      int limit = data.value("limit", 200);
      if (limit > 200) limit = 200;
      if (limit < 1) limit = 1;
      if (peerType == "p2p") {
        auto rows = db_.historyP2p(uid, peerId, limit);
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& r : rows) {
          arr.push_back({{"id", r.id},
                         {"fromUid", r.fromUid},
                         {"toUid", r.toUid},
                         {"content", r.content},
                         {"msgType", r.msgType},
                         {"createTime", r.createTime}});
        }
        reply(fd, envOk("msg.history", {{"peerType", "p2p"}, {"peerId", peerId}, {"messages", arr}}));
        return;
      }
      if (peerType == "group") {
        auto rows = db_.historyGroup(peerId, limit);
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& r : rows) {
          arr.push_back({{"id", r.id},
                         {"groupId", r.groupId},
                         {"fromUid", r.fromUid},
                         {"content", r.content},
                         {"msgType", r.msgType},
                         {"createTime", r.createTime}});
        }
        reply(fd, envOk("msg.history", {{"peerType", "group"}, {"peerId", peerId}, {"messages", arr}}));
        return;
      }
      reply(fd, envErr("msg.history", "参数错误"));
      return;
    }

    if (type == "msg.offline") {
      int cnt = 0;
      if (net_) {
        cnt = redis_.offlineDrainAndSend(uid, [this, fd](const std::string& line) {
          try {
            auto je = nlohmann::json::parse(line);
            net_->sendWsJson(fd, je);
          } catch (...) {
          }
        });
      }
      reply(fd, envOk("msg.offline", {{"count", cnt}}));
      return;
    }

    if (type == "msg.read") {
      std::string peerType = data.value("peerType", "");
      int64_t peerId = data.value("peerId", 0);
      if (peerType == "p2p") {
        redis_.clearUnreadP2p(uid, peerId);
        db_.markP2pRead(uid, peerId);
      } else if (peerType == "group") {
        redis_.clearUnreadGroup(uid, peerId);
      }
      reply(fd, envOk("msg.read", {{"ok", true}, {"peerType", peerType}, {"peerId", peerId}}));
      return;
    }

    if (type == "sync.init") {
      auto friends = db_.listFriends(uid);
      auto groups = db_.listGroups(uid);
      nlohmann::json unread = redis_.unreadSnapshot(uid);
      int pending = db_.countPendingApplies(uid);
      bool bell = redis_.applyBellShouldShow(uid, pending);
      nlohmann::json fr = nlohmann::json::array();
      for (const auto& f : friends) fr.push_back({{"id", f.id}, {"username", f.username}, {"nickname", f.nickname}});
      nlohmann::json gr = nlohmann::json::array();
      for (const auto& g : groups)
        gr.push_back({{"id", g.id}, {"group_name", g.group_name}, {"owner_id", g.owner_id}});
      reply(fd, envOk("sync.init",
                      {{"friends", fr}, {"groups", gr}, {"unread", unread}, {"applyBell", bell}, {"pendingApplyCount", pending}}));
      return;
    }

    reply(fd, envErr("error", "未知类型: " + type));
  } catch (const std::exception& e) {
    reply(fd, envErr("error", e.what()));
  }
}
