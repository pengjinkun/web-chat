#include "RedisStore.h"
#include "../Logger.h"

#include <hiredis/hiredis.h>

#include <cstdio>
#include <cstring>

namespace {

redisContext* R(void* p) { return reinterpret_cast<redisContext*>(p); }

}  // namespace

RedisStore::RedisStore(std::string host, int port) : host_(std::move(host)), port_(port) {}

RedisStore::~RedisStore() {
  if (redis_) {
    redisFree(R(redis_));
    redis_ = nullptr;
  }
}

bool RedisStore::connect() {
  std::lock_guard<std::mutex> lk(mtx_);
  if (redis_) {
    redisFree(R(redis_));
    redis_ = nullptr;
  }
  redisContext* c = redisConnect(host_.c_str(), port_);
  if (!c || c->err) {
    logErr(c && c->errstr ? c->errstr : "redisConnect failed");
    if (c) redisFree(c);
    return false;
  }
  redis_ = c;
  return true;
}

void RedisStore::userOnline(int64_t uid, int64_t connectTimeMs) {
  std::lock_guard<std::mutex> lk(mtx_);
  if (!redis_) return;
  char uidbuf[32];
  std::snprintf(uidbuf, sizeof uidbuf, "%lld", static_cast<long long>(uid));
  redisReply* rep =
      static_cast<redisReply*>(redisCommand(R(redis_), "SET user:online:%s 1", uidbuf));
  if (rep) freeReplyObject(rep);
  rep = static_cast<redisReply*>(
      redisCommand(R(redis_), "ZADD online_users %lld %s", static_cast<long long>(connectTimeMs), uidbuf));
  if (rep) freeReplyObject(rep);
}

void RedisStore::userOffline(int64_t uid) {
  std::lock_guard<std::mutex> lk(mtx_);
  if (!redis_) return;
  char uidbuf[32];
  std::snprintf(uidbuf, sizeof uidbuf, "%lld", static_cast<long long>(uid));
  redisReply* rep =
      static_cast<redisReply*>(redisCommand(R(redis_), "DEL user:online:%s", uidbuf));
  if (rep) freeReplyObject(rep);
  rep = static_cast<redisReply*>(redisCommand(R(redis_), "ZREM online_users %s", uidbuf));
  if (rep) freeReplyObject(rep);
}

void RedisStore::applyBellClear(int64_t uid) {
  std::lock_guard<std::mutex> lk(mtx_);
  if (!redis_) return;
  char k[64];
  std::snprintf(k, sizeof k, "apply_hide:%lld", static_cast<long long>(uid));
  redisReply* rep = static_cast<redisReply*>(redisCommand(R(redis_), "SET %s 1", k));
  if (rep) freeReplyObject(rep);
}

void RedisStore::applyBellResetForTarget(int64_t uid) {
  std::lock_guard<std::mutex> lk(mtx_);
  if (!redis_) return;
  char k[64];
  std::snprintf(k, sizeof k, "apply_hide:%lld", static_cast<long long>(uid));
  redisReply* rep = static_cast<redisReply*>(redisCommand(R(redis_), "DEL %s", k));
  if (rep) freeReplyObject(rep);
}

bool RedisStore::applyBellShouldShow(int64_t uid, int pendingMysqlCount) {
  if (pendingMysqlCount <= 0) return false;
  std::lock_guard<std::mutex> lk(mtx_);
  if (!redis_) return true;
  char k[64];
  std::snprintf(k, sizeof k, "apply_hide:%lld", static_cast<long long>(uid));
  redisReply* rep = static_cast<redisReply*>(redisCommand(R(redis_), "EXISTS %s", k));
  bool show = true;
  if (rep && rep->type == REDIS_REPLY_INTEGER) show = (rep->integer == 0);
  if (rep) freeReplyObject(rep);
  return show;
}

void RedisStore::incrUnreadP2p(int64_t uid, int64_t fromUid) {
  std::lock_guard<std::mutex> lk(mtx_);
  if (!redis_) return;
  char k[96];
  std::snprintf(k, sizeof k, "unread:p2p:%lld:%lld", static_cast<long long>(uid),
                static_cast<long long>(fromUid));
  redisReply* rep = static_cast<redisReply*>(redisCommand(R(redis_), "INCR %s", k));
  if (rep) freeReplyObject(rep);
}

void RedisStore::clearUnreadP2p(int64_t uid, int64_t peerFid) {
  std::lock_guard<std::mutex> lk(mtx_);
  if (!redis_) return;
  char k[96];
  std::snprintf(k, sizeof k, "unread:p2p:%lld:%lld", static_cast<long long>(uid),
                static_cast<long long>(peerFid));
  redisReply* rep = static_cast<redisReply*>(redisCommand(R(redis_), "DEL %s", k));
  if (rep) freeReplyObject(rep);
}

void RedisStore::incrUnreadGroup(int64_t uid, int64_t gid) {
  std::lock_guard<std::mutex> lk(mtx_);
  if (!redis_) return;
  char k[96];
  std::snprintf(k, sizeof k, "unread:group:%lld:%lld", static_cast<long long>(uid),
                static_cast<long long>(gid));
  redisReply* rep = static_cast<redisReply*>(redisCommand(R(redis_), "INCR %s", k));
  if (rep) freeReplyObject(rep);
}

void RedisStore::clearUnreadGroup(int64_t uid, int64_t gid) {
  std::lock_guard<std::mutex> lk(mtx_);
  if (!redis_) return;
  char k[96];
  std::snprintf(k, sizeof k, "unread:group:%lld:%lld", static_cast<long long>(uid),
                static_cast<long long>(gid));
  redisReply* rep = static_cast<redisReply*>(redisCommand(R(redis_), "DEL %s", k));
  if (rep) freeReplyObject(rep);
}

nlohmann::json RedisStore::unreadSnapshot(int64_t uid) {
  nlohmann::json out;
  out["p2p"] = nlohmann::json::object();
  out["group"] = nlohmann::json::object();
  std::lock_guard<std::mutex> lk(mtx_);
  if (!redis_) return out;
  char patP2p[64];
  std::snprintf(patP2p, sizeof patP2p, "unread:p2p:%lld:*", static_cast<long long>(uid));
  redisReply* rep = static_cast<redisReply*>(redisCommand(R(redis_), "KEYS %s", patP2p));
  if (rep && rep->type == REDIS_REPLY_ARRAY) {
    for (size_t i = 0; i < rep->elements; ++i) {
      const char* key = rep->element[i]->str;
      if (!key) continue;
      const char* last = std::strrchr(key, ':');
      if (!last) continue;
      std::string fid(last + 1);
      redisReply* g = static_cast<redisReply*>(redisCommand(R(redis_), "GET %s", key));
      if (g && g->type == REDIS_REPLY_STRING && g->str) {
        long n = std::atol(g->str);
        if (n > 0) out["p2p"][fid] = n;
      }
      if (g) freeReplyObject(g);
    }
  }
  if (rep) freeReplyObject(rep);

  char patG[64];
  std::snprintf(patG, sizeof patG, "unread:group:%lld:*", static_cast<long long>(uid));
  rep = static_cast<redisReply*>(redisCommand(R(redis_), "KEYS %s", patG));
  if (rep && rep->type == REDIS_REPLY_ARRAY) {
    for (size_t i = 0; i < rep->elements; ++i) {
      const char* key = rep->element[i]->str;
      if (!key) continue;
      const char* last = std::strrchr(key, ':');
      if (!last) continue;
      std::string gid(last + 1);
      redisReply* g = static_cast<redisReply*>(redisCommand(R(redis_), "GET %s", key));
      if (g && g->type == REDIS_REPLY_STRING && g->str) {
        long n = std::atol(g->str);
        if (n > 0) out["group"][gid] = n;
      }
      if (g) freeReplyObject(g);
    }
  }
  if (rep) freeReplyObject(rep);
  return out;
}

void RedisStore::offlinePush(int64_t uid, const std::string& envelopeJson) {
  std::lock_guard<std::mutex> lk(mtx_);
  if (!redis_) return;
  char k[48];
  std::snprintf(k, sizeof k, "offline:msg:%lld", static_cast<long long>(uid));
  redisReply* rep =
      static_cast<redisReply*>(redisCommand(R(redis_), "RPUSH %s %b", k, envelopeJson.data(),
                                            static_cast<size_t>(envelopeJson.size())));
  if (rep) freeReplyObject(rep);
}

int RedisStore::offlineDrainAndSend(int64_t uid, const OfflineSender& send) {
  std::lock_guard<std::mutex> lk(mtx_);
  if (!redis_) return 0;
  char k[48];
  std::snprintf(k, sizeof k, "offline:msg:%lld", static_cast<long long>(uid));
  redisReply* rep = static_cast<redisReply*>(redisCommand(R(redis_), "LRANGE %s 0 -1", k));
  int cnt = 0;
  if (rep && rep->type == REDIS_REPLY_ARRAY) {
    cnt = static_cast<int>(rep->elements);
    for (size_t i = 0; i < rep->elements; ++i) {
      if (rep->element[i]->str)
        send(std::string(rep->element[i]->str, rep->element[i]->len));
    }
  }
  if (rep) freeReplyObject(rep);
  rep = static_cast<redisReply*>(redisCommand(R(redis_), "DEL %s", k));
  if (rep) freeReplyObject(rep);
  return cnt;
}

std::vector<std::pair<int64_t, int64_t>> RedisStore::recommendByOnlineOrder(size_t maxMembers) {
  std::vector<std::pair<int64_t, int64_t>> out;
  std::lock_guard<std::mutex> lk(mtx_);
  if (!redis_) return out;
  long last = maxMembers == 0 ? 199 : static_cast<long>(maxMembers) - 1;
  if (last < 0) last = 0;
  redisReply* rep =
      static_cast<redisReply*>(redisCommand(R(redis_), "ZRANGE online_users 0 %ld WITHSCORES", last));
  if (rep && rep->type == REDIS_REPLY_ARRAY) {
    for (size_t i = 0; i + 1 < rep->elements; i += 2) {
      if (!rep->element[i]->str || !rep->element[i + 1]->str) continue;
      int64_t uid = std::atoll(rep->element[i]->str);
      int64_t ms = std::atoll(rep->element[i + 1]->str);
      out.emplace_back(uid, ms);
    }
  }
  if (rep) freeReplyObject(rep);
  return out;
}
