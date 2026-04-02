#include "MysqlStore.h"
#include "../Logger.h"

#include <mysql/mysql.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

MYSQL* M(void* p) { return reinterpret_cast<MYSQL*>(p); }

}  // namespace

MysqlStore::MysqlStore(std::string host, unsigned port, std::string user, std::string pass, std::string db)
    : host_(std::move(host)),
      port_(port),
      user_(std::move(user)),
      pass_(std::move(pass)),
      db_(std::move(db)) {
  mysql_ = mysql_init(nullptr);
}

MysqlStore::~MysqlStore() {
  if (mysql_) {
    mysql_close(M(mysql_));
    mysql_ = nullptr;
  }
}

bool MysqlStore::connect() {
  std::lock_guard<std::mutex> lk(mtx_);
  if (!mysql_) mysql_ = mysql_init(nullptr);
  if (!mysql_real_connect(M(mysql_), host_.c_str(), user_.c_str(), pass_.c_str(), db_.c_str(), port_, nullptr, 0)) {
    logErr(mysql_error(M(mysql_)));
    return false;
  }
  if (mysql_set_character_set(M(mysql_), "utf8mb4")) {
    logErr("mysql_set_character_set utf8mb4 failed");
    return false;
  }
  return true;
}

std::string MysqlStore::escape(const std::string& s) {
  std::vector<char> b(s.size() * 2 + 1);
  unsigned long l = mysql_real_escape_string(M(mysql_), b.data(), s.c_str(), s.size());
  return std::string(b.data(), l);
}

bool MysqlStore::userExistsByUsername(const std::string& username) {
  std::lock_guard<std::mutex> lk(mtx_);
  std::string e = escape(username);
  std::string sql = "SELECT id FROM user WHERE username='" + e + "' LIMIT 1";
  if (mysql_query(M(mysql_), sql.c_str())) {
    logErr(mysql_error(M(mysql_)));
    return false;
  }
  MYSQL_RES* res = mysql_store_result(M(mysql_));
  bool ex = res && mysql_fetch_row(res);
  if (res) mysql_free_result(res);
  return ex;
}

bool MysqlStore::insertUser(const std::string& username, const std::string& passMd5, const std::string& nickname) {
  std::lock_guard<std::mutex> lk(mtx_);
  std::string eu = escape(username);
  std::string ep = escape(passMd5);
  std::string en = escape(nickname);
  std::string sql =
      "INSERT INTO user (username,password,nickname) VALUES ('" + eu + "','" + ep + "','" + en + "')";
  if (mysql_query(M(mysql_), sql.c_str())) {
    logErr(mysql_error(M(mysql_)));
    return false;
  }
  return true;
}

bool MysqlStore::getUserByUsername(const std::string& username, UserRow& out) {
  std::lock_guard<std::mutex> lk(mtx_);
  std::string e = escape(username);
  std::string sql =
      "SELECT id,username,nickname,password FROM user WHERE username='" + e + "' LIMIT 1";
  if (mysql_query(M(mysql_), sql.c_str())) {
    logErr(mysql_error(M(mysql_)));
    return false;
  }
  MYSQL_RES* res = mysql_store_result(M(mysql_));
  if (!res) return false;
  MYSQL_ROW row = mysql_fetch_row(res);
  if (!row) {
    mysql_free_result(res);
    return false;
  }
  out.id = row[0] ? std::atoll(row[0]) : 0;
  out.username = row[1] ? row[1] : "";
  out.nickname = row[2] ? row[2] : "";
  out.password = row[3] ? row[3] : "";
  mysql_free_result(res);
  return true;
}

bool MysqlStore::getUserById(int64_t id, UserRow& out) {
  std::lock_guard<std::mutex> lk(mtx_);
  char sql[256];
  std::snprintf(sql, sizeof sql, "SELECT id,username,nickname,password FROM user WHERE id=%lld LIMIT 1",
                static_cast<long long>(id));
  if (mysql_query(M(mysql_), sql)) {
    logErr(mysql_error(M(mysql_)));
    return false;
  }
  MYSQL_RES* res = mysql_store_result(M(mysql_));
  if (!res) return false;
  MYSQL_ROW row = mysql_fetch_row(res);
  if (!row) {
    mysql_free_result(res);
    return false;
  }
  out.id = row[0] ? std::atoll(row[0]) : 0;
  out.username = row[1] ? row[1] : "";
  out.nickname = row[2] ? row[2] : "";
  out.password = row[3] ? row[3] : "";
  mysql_free_result(res);
  return true;
}

bool MysqlStore::isFriend(int64_t a, int64_t b) {
  std::lock_guard<std::mutex> lk(mtx_);
  char sql[256];
  std::snprintf(sql, sizeof sql,
                "SELECT 1 FROM friend_relation WHERE user_id=%lld AND friend_id=%lld AND status=1 LIMIT 1",
                static_cast<long long>(a), static_cast<long long>(b));
  if (mysql_query(M(mysql_), sql)) {
    logErr(mysql_error(M(mysql_)));
    return false;
  }
  MYSQL_RES* res = mysql_store_result(M(mysql_));
  bool ok = res && mysql_fetch_row(res);
  if (res) mysql_free_result(res);
  return ok;
}

bool MysqlStore::insertFriendApply(int64_t fromUid, int64_t toUid) {
  std::lock_guard<std::mutex> lk(mtx_);
  char sql[256];
  std::snprintf(sql, sizeof sql, "INSERT INTO friend_apply (from_uid,to_uid,status) VALUES (%lld,%lld,0)",
                static_cast<long long>(fromUid), static_cast<long long>(toUid));
  return mysql_query(M(mysql_), sql) == 0;
}

bool MysqlStore::hasPendingApply(int64_t fromUid, int64_t toUid) {
  std::lock_guard<std::mutex> lk(mtx_);
  char sql[256];
  std::snprintf(sql, sizeof sql,
                "SELECT id FROM friend_apply WHERE from_uid=%lld AND to_uid=%lld AND status=0 LIMIT 1",
                static_cast<long long>(fromUid), static_cast<long long>(toUid));
  if (mysql_query(M(mysql_), sql)) return false;
  MYSQL_RES* res = mysql_store_result(M(mysql_));
  bool ok = res && mysql_fetch_row(res);
  if (res) mysql_free_result(res);
  return ok;
}

bool MysqlStore::getFriendApply(int64_t applyId, int64_t toUid, int64_t& outFrom, int& outStatus) {
  std::lock_guard<std::mutex> lk(mtx_);
  char sql[256];
  std::snprintf(sql, sizeof sql, "SELECT from_uid,status FROM friend_apply WHERE id=%lld AND to_uid=%lld LIMIT 1",
                static_cast<long long>(applyId), static_cast<long long>(toUid));
  if (mysql_query(M(mysql_), sql)) return false;
  MYSQL_RES* res = mysql_store_result(M(mysql_));
  if (!res) return false;
  MYSQL_ROW row = mysql_fetch_row(res);
  if (!row) {
    mysql_free_result(res);
    return false;
  }
  outFrom = row[0] ? std::atoll(row[0]) : 0;
  outStatus = row[1] ? std::atoi(row[1]) : -1;
  mysql_free_result(res);
  return true;
}

bool MysqlStore::setFriendApplyStatus(int64_t applyId, int status) {
  std::lock_guard<std::mutex> lk(mtx_);
  char sql[128];
  std::snprintf(sql, sizeof sql, "UPDATE friend_apply SET status=%d WHERE id=%lld", status,
                static_cast<long long>(applyId));
  return mysql_query(M(mysql_), sql) == 0;
}

bool MysqlStore::insertFriendRelationIgnore(int64_t userId, int64_t friendId) {
  std::lock_guard<std::mutex> lk(mtx_);
  char sql[256];
  std::snprintf(sql, sizeof sql,
                "INSERT IGNORE INTO friend_relation (user_id,friend_id,status) VALUES (%lld,%lld,1)",
                static_cast<long long>(userId), static_cast<long long>(friendId));
  return mysql_query(M(mysql_), sql) == 0;
}

int MysqlStore::countPendingApplies(int64_t toUid) {
  std::lock_guard<std::mutex> lk(mtx_);
  char sql[128];
  std::snprintf(sql, sizeof sql, "SELECT COUNT(*) FROM friend_apply WHERE to_uid=%lld AND status=0",
                static_cast<long long>(toUid));
  if (mysql_query(M(mysql_), sql)) return 0;
  MYSQL_RES* res = mysql_store_result(M(mysql_));
  if (!res) return 0;
  MYSQL_ROW row = mysql_fetch_row(res);
  int c = row && row[0] ? std::atoi(row[0]) : 0;
  mysql_free_result(res);
  return c;
}

std::vector<ApplyRow> MysqlStore::listFriendApplies(int64_t toUid) {
  std::lock_guard<std::mutex> lk(mtx_);
  std::vector<ApplyRow> out;
  char sql[512];
  std::snprintf(sql, sizeof sql,
                "SELECT fa.id,fa.from_uid,fa.status,fa.create_time,u.username,u.nickname "
                "FROM friend_apply fa JOIN user u ON u.id=fa.from_uid WHERE fa.to_uid=%lld ORDER BY fa.create_time DESC",
                static_cast<long long>(toUid));
  if (mysql_query(M(mysql_), sql)) return out;
  MYSQL_RES* res = mysql_store_result(M(mysql_));
  if (!res) return out;
  MYSQL_ROW row;
  while ((row = mysql_fetch_row(res))) {
    ApplyRow r;
    r.id = row[0] ? std::atoll(row[0]) : 0;
    r.from_uid = row[1] ? std::atoll(row[1]) : 0;
    r.status = row[2] ? std::atoi(row[2]) : 0;
    r.create_time = row[3] ? row[3] : "";
    r.username = row[4] ? row[4] : "";
    r.nickname = row[5] ? row[5] : "";
    out.push_back(std::move(r));
  }
  mysql_free_result(res);
  return out;
}

std::vector<FriendRow> MysqlStore::listFriends(int64_t uid) {
  std::lock_guard<std::mutex> lk(mtx_);
  std::vector<FriendRow> out;
  char sql[512];
  std::snprintf(sql, sizeof sql,
                "SELECT u.id,u.username,u.nickname FROM friend_relation fr "
                "JOIN user u ON u.id=fr.friend_id WHERE fr.user_id=%lld AND fr.status=1",
                static_cast<long long>(uid));
  if (mysql_query(M(mysql_), sql)) return out;
  MYSQL_RES* res = mysql_store_result(M(mysql_));
  if (!res) return out;
  MYSQL_ROW row;
  while ((row = mysql_fetch_row(res))) {
    FriendRow r;
    r.id = row[0] ? std::atoll(row[0]) : 0;
    r.username = row[1] ? row[1] : "";
    r.nickname = row[2] ? row[2] : "";
    out.push_back(std::move(r));
  }
  mysql_free_result(res);
  return out;
}

std::vector<GroupRow> MysqlStore::listGroups(int64_t uid) {
  std::lock_guard<std::mutex> lk(mtx_);
  std::vector<GroupRow> out;
  char sql[512];
  std::snprintf(sql, sizeof sql,
                "SELECT g.id,g.group_name,g.owner_id FROM `group` g "
                "JOIN group_member gm ON gm.group_id=g.id WHERE gm.user_id=%lld",
                static_cast<long long>(uid));
  if (mysql_query(M(mysql_), sql)) return out;
  MYSQL_RES* res = mysql_store_result(M(mysql_));
  if (!res) return out;
  MYSQL_ROW row;
  while ((row = mysql_fetch_row(res))) {
    GroupRow r;
    r.id = row[0] ? std::atoll(row[0]) : 0;
    r.group_name = row[1] ? row[1] : "";
    r.owner_id = row[2] ? std::atoll(row[2]) : 0;
    out.push_back(std::move(r));
  }
  mysql_free_result(res);
  return out;
}

std::vector<FriendRow> MysqlStore::searchUsers(const std::string& likePattern, int64_t excludeUid) {
  std::lock_guard<std::mutex> lk(mtx_);
  std::vector<FriendRow> out;
  std::string e = escape(likePattern);
  std::string sql = "SELECT id,username,nickname FROM user WHERE username LIKE '%" + e +
                    "%' AND id != " + std::to_string(excludeUid) + " LIMIT 30";
  if (mysql_query(M(mysql_), sql.c_str())) return out;
  MYSQL_RES* res = mysql_store_result(M(mysql_));
  if (!res) return out;
  MYSQL_ROW row;
  while ((row = mysql_fetch_row(res))) {
    FriendRow r;
    r.id = row[0] ? std::atoll(row[0]) : 0;
    r.username = row[1] ? row[1] : "";
    r.nickname = row[2] ? row[2] : "";
    out.push_back(std::move(r));
  }
  mysql_free_result(res);
  return out;
}

std::vector<GroupRow> MysqlStore::searchGroups(const std::string& likePattern) {
  std::lock_guard<std::mutex> lk(mtx_);
  std::vector<GroupRow> out;
  std::string e = escape(likePattern);
  std::string sql = "SELECT id,group_name,owner_id FROM `group` WHERE group_name LIKE '%" + e +
                    "%' OR CAST(id AS CHAR) LIKE '%" + e + "%' LIMIT 30";
  if (mysql_query(M(mysql_), sql.c_str())) return out;
  MYSQL_RES* res = mysql_store_result(M(mysql_));
  if (!res) return out;
  MYSQL_ROW row;
  while ((row = mysql_fetch_row(res))) {
    GroupRow r;
    r.id = row[0] ? std::atoll(row[0]) : 0;
    r.group_name = row[1] ? row[1] : "";
    r.owner_id = row[2] ? std::atoll(row[2]) : 0;
    out.push_back(std::move(r));
  }
  mysql_free_result(res);
  return out;
}

int64_t MysqlStore::insertGroup(const std::string& name, int64_t ownerId) {
  std::lock_guard<std::mutex> lk(mtx_);
  std::string en = escape(name);
  std::string sql =
      "INSERT INTO `group` (group_name,owner_id) VALUES ('" + en + "'," + std::to_string(ownerId) + ")";
  if (mysql_query(M(mysql_), sql.c_str())) return 0;
  return static_cast<int64_t>(mysql_insert_id(M(mysql_)));
}

bool MysqlStore::insertGroupMember(int64_t gid, int64_t uid) {
  std::lock_guard<std::mutex> lk(mtx_);
  char sql[256];
  std::snprintf(sql, sizeof sql, "INSERT INTO group_member (group_id,user_id) VALUES (%lld,%lld)",
                static_cast<long long>(gid), static_cast<long long>(uid));
  return mysql_query(M(mysql_), sql) == 0;
}

bool MysqlStore::getGroupById(int64_t gid, GroupRow& out) {
  std::lock_guard<std::mutex> lk(mtx_);
  char sql[128];
  std::snprintf(sql, sizeof sql, "SELECT id,group_name,owner_id FROM `group` WHERE id=%lld LIMIT 1",
                static_cast<long long>(gid));
  if (mysql_query(M(mysql_), sql)) return false;
  MYSQL_RES* res = mysql_store_result(M(mysql_));
  if (!res) return false;
  MYSQL_ROW row = mysql_fetch_row(res);
  if (!row) {
    mysql_free_result(res);
    return false;
  }
  out.id = row[0] ? std::atoll(row[0]) : 0;
  out.group_name = row[1] ? row[1] : "";
  out.owner_id = row[2] ? std::atoll(row[2]) : 0;
  mysql_free_result(res);
  return true;
}

bool MysqlStore::isGroupMember(int64_t gid, int64_t uid) {
  std::lock_guard<std::mutex> lk(mtx_);
  char sql[256];
  std::snprintf(sql, sizeof sql,
                "SELECT 1 FROM group_member WHERE group_id=%lld AND user_id=%lld LIMIT 1",
                static_cast<long long>(gid), static_cast<long long>(uid));
  if (mysql_query(M(mysql_), sql)) return false;
  MYSQL_RES* res = mysql_store_result(M(mysql_));
  bool ok = res && mysql_fetch_row(res);
  if (res) mysql_free_result(res);
  return ok;
}

std::vector<int64_t> MysqlStore::listGroupMemberIds(int64_t gid) {
  std::lock_guard<std::mutex> lk(mtx_);
  std::vector<int64_t> out;
  char sql[128];
  std::snprintf(sql, sizeof sql, "SELECT user_id FROM group_member WHERE group_id=%lld",
                static_cast<long long>(gid));
  if (mysql_query(M(mysql_), sql)) return out;
  MYSQL_RES* res = mysql_store_result(M(mysql_));
  if (!res) return out;
  MYSQL_ROW row;
  while ((row = mysql_fetch_row(res))) {
    if (row[0]) out.push_back(std::atoll(row[0]));
  }
  mysql_free_result(res);
  return out;
}

int64_t MysqlStore::insertP2pMessage(int64_t fromUid, int64_t toUid, const std::string& content, int msgType) {
  std::lock_guard<std::mutex> lk(mtx_);
  std::string ec = escape(content);
  std::string sql = "INSERT INTO p2p_message (from_uid,to_uid,content,msg_type,is_read) VALUES (" +
                    std::to_string(fromUid) + "," + std::to_string(toUid) + ",'" + ec + "'," +
                    std::to_string(msgType) + ",0)";
  if (mysql_query(M(mysql_), sql.c_str())) return 0;
  return static_cast<int64_t>(mysql_insert_id(M(mysql_)));
}

int64_t MysqlStore::insertGroupMessage(int64_t gid, int64_t fromUid, const std::string& content, int msgType) {
  std::lock_guard<std::mutex> lk(mtx_);
  std::string ec = escape(content);
  std::string sql = "INSERT INTO group_message (group_id,from_uid,content,msg_type) VALUES (" +
                    std::to_string(gid) + "," + std::to_string(fromUid) + ",'" + ec + "'," +
                    std::to_string(msgType) + ")";
  if (mysql_query(M(mysql_), sql.c_str())) return 0;
  return static_cast<int64_t>(mysql_insert_id(M(mysql_)));
}

static std::string isoTime(const char* mysqlDt) {
  if (!mysqlDt || !*mysqlDt) return "";
  std::string s = mysqlDt;
  for (char& c : s)
    if (c == ' ') c = 'T';
  if (s.size() >= 19 && s[10] == 'T') return s.substr(0, 19) + ".000Z";
  return s;
}

std::vector<P2pMsgRow> MysqlStore::historyP2p(int64_t uid, int64_t peer, int limit) {
  std::lock_guard<std::mutex> lk(mtx_);
  std::vector<P2pMsgRow> out;
  char sql[512];
  std::snprintf(sql, sizeof sql,
                "SELECT id,from_uid,to_uid,content,msg_type,create_time FROM p2p_message "
                "WHERE (from_uid=%lld AND to_uid=%lld) OR (from_uid=%lld AND to_uid=%lld) "
                "ORDER BY id DESC LIMIT %d",
                static_cast<long long>(uid), static_cast<long long>(peer),
                static_cast<long long>(peer), static_cast<long long>(uid), limit);
  if (mysql_query(M(mysql_), sql)) return out;
  MYSQL_RES* res = mysql_store_result(M(mysql_));
  if (!res) return out;
  MYSQL_ROW row;
  while ((row = mysql_fetch_row(res))) {
    P2pMsgRow r;
    r.id = row[0] ? std::atoll(row[0]) : 0;
    r.fromUid = row[1] ? std::atoll(row[1]) : 0;
    r.toUid = row[2] ? std::atoll(row[2]) : 0;
    r.content = row[3] ? row[3] : "";
    r.msgType = row[4] ? std::atoi(row[4]) : 1;
    r.createTime = isoTime(row[5]);
    out.push_back(std::move(r));
  }
  mysql_free_result(res);
  std::reverse(out.begin(), out.end());
  return out;
}

std::vector<GroupMsgRow> MysqlStore::historyGroup(int64_t gid, int limit) {
  std::lock_guard<std::mutex> lk(mtx_);
  std::vector<GroupMsgRow> out;
  char sql[256];
  std::snprintf(sql, sizeof sql,
                "SELECT id,group_id,from_uid,content,msg_type,create_time FROM group_message "
                "WHERE group_id=%lld ORDER BY id DESC LIMIT %d",
                static_cast<long long>(gid), limit);
  if (mysql_query(M(mysql_), sql)) return out;
  MYSQL_RES* res = mysql_store_result(M(mysql_));
  if (!res) return out;
  MYSQL_ROW row;
  while ((row = mysql_fetch_row(res))) {
    GroupMsgRow r;
    r.id = row[0] ? std::atoll(row[0]) : 0;
    r.groupId = row[1] ? std::atoll(row[1]) : 0;
    r.fromUid = row[2] ? std::atoll(row[2]) : 0;
    r.content = row[3] ? row[3] : "";
    r.msgType = row[4] ? std::atoi(row[4]) : 1;
    r.createTime = isoTime(row[5]);
    out.push_back(std::move(r));
  }
  mysql_free_result(res);
  std::reverse(out.begin(), out.end());
  return out;
}

bool MysqlStore::markP2pRead(int64_t uid, int64_t fromUid) {
  std::lock_guard<std::mutex> lk(mtx_);
  char sql[256];
  std::snprintf(sql, sizeof sql,
                "UPDATE p2p_message SET is_read=1 WHERE to_uid=%lld AND from_uid=%lld",
                static_cast<long long>(uid), static_cast<long long>(fromUid));
  return mysql_query(M(mysql_), sql) == 0;
}

std::vector<int64_t> MysqlStore::listFriendIds(int64_t uid) {
  std::lock_guard<std::mutex> lk(mtx_);
  std::vector<int64_t> out;
  char sql[128];
  std::snprintf(sql, sizeof sql, "SELECT friend_id FROM friend_relation WHERE user_id=%lld AND status=1",
                static_cast<long long>(uid));
  if (mysql_query(M(mysql_), sql)) return out;
  MYSQL_RES* res = mysql_store_result(M(mysql_));
  if (!res) return out;
  MYSQL_ROW row;
  while ((row = mysql_fetch_row(res))) {
    if (row[0]) out.push_back(std::atoll(row[0]));
  }
  mysql_free_result(res);
  return out;
}
