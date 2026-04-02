#include "EpollServer.h"
#include "SocketUtil.h"
#include "WebSocketUtil.h"
#include "WsConnection.h"
#include "../chat/ChatHandler.h"
#include "../Logger.h"

#include <nlohmann/json.hpp>

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <algorithm>
#include <cerrno>
#include <cstring>

namespace {

constexpr int kMaxEvents = 256;

void setEpollOut(int epfd, int fd, bool enable) {
  epoll_event ev{};
  ev.data.fd = fd;
  ev.events = enable ? (EPOLLIN | EPOLLOUT) : EPOLLIN;
  epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
}

}  // namespace

struct EpollServer::ClientCtx {
  std::unique_ptr<WsConnection> ws;
  std::string outbuf;
};

EpollServer::EpollServer(ChatHandler& chat) : chat_(chat) {}

EpollServer::~EpollServer() {
  stop();
  join();
}

bool EpollServer::start(uint16_t port) {
  listenFd_ = createListenSocket(port);
  if (listenFd_ < 0) return false;
  epfd_ = epoll_create1(0);
  if (epfd_ < 0) {
    logErr("epoll_create1");
    close(listenFd_);
    listenFd_ = -1;
    return false;
  }
  wakeFd_ = eventfd(0, EFD_NONBLOCK);
  if (wakeFd_ < 0) {
    logErr("eventfd");
    close(epfd_);
    close(listenFd_);
    epfd_ = -1;
    listenFd_ = -1;
    return false;
  }
  epoll_event ev{};
  ev.events = EPOLLIN;
  ev.data.fd = wakeFd_;
  if (epoll_ctl(epfd_, EPOLL_CTL_ADD, wakeFd_, &ev) < 0) {
    logErr("epoll_ctl wake");
    close(wakeFd_);
    close(epfd_);
    close(listenFd_);
    wakeFd_ = epfd_ = listenFd_ = -1;
    return false;
  }
  running_ = true;
  bossThread_ = std::thread(&EpollServer::bossLoop, this);
  workerThread_ = std::thread(&EpollServer::workerLoop, this);
  logInfo("Epoll server starting (Boss accept + Worker epoll)");
  return true;
}

void EpollServer::stop() {
  running_ = false;
  if (listenFd_ >= 0) {
    shutdown(listenFd_, SHUT_RDWR);
    close(listenFd_);
    listenFd_ = -1;
  }
  if (wakeFd_ >= 0) {
    uint64_t one = 1;
    (void)::write(wakeFd_, &one, sizeof one);
  }
}

void EpollServer::join() {
  if (bossThread_.joinable()) bossThread_.join();
  if (workerThread_.joinable()) workerThread_.join();
  if (wakeFd_ >= 0) {
    close(wakeFd_);
    wakeFd_ = -1;
  }
  if (epfd_ >= 0) {
    close(epfd_);
    epfd_ = -1;
  }
}

void EpollServer::bossLoop() {
  while (running_) {
    sockaddr_in cli {};
    socklen_t len = sizeof cli;
    int cfd = ::accept(listenFd_, reinterpret_cast<sockaddr*>(&cli), &len);
    if (cfd < 0) {
      if (!running_) break;
      if (errno == EINTR) continue;
      break;
    }
    if (setNonBlocking(cfd) < 0) {
      close(cfd);
      continue;
    }
    {
      std::lock_guard<std::mutex> lk(pendingMtx_);
      pendingFds_.push_back(cfd);
    }
    uint64_t one = 1;
    (void)::write(wakeFd_, &one, sizeof one);
  }
}

void EpollServer::enqueueRaw(int fd, const std::string& data) {
  std::lock_guard<std::mutex> lk(connMtx_);
  auto it = conns_.find(fd);
  if (it == conns_.end()) return;
  bool wasEmpty = it->second->outbuf.empty();
  it->second->outbuf += data;
  if (wasEmpty && !it->second->outbuf.empty()) setEpollOut(epfd_, fd, true);
}

void EpollServer::flushOut(int fd) {
  std::lock_guard<std::mutex> lk(connMtx_);
  auto it = conns_.find(fd);
  if (it == conns_.end()) return;
  std::string& ob = it->second->outbuf;
  while (!ob.empty()) {
    ssize_t w = ::write(fd, ob.data(), ob.size());
    if (w > 0) ob.erase(0, static_cast<size_t>(w));
    else if (w < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
    else {
      ob.clear();
      break;
    }
  }
  if (ob.empty()) setEpollOut(epfd_, fd, false);
}

void EpollServer::closeClient(int fd) {
  chat_.onDisconnect(fd);
  epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr);
  {
    std::lock_guard<std::mutex> lk(connMtx_);
    conns_.erase(fd);
  }
  close(fd);
}

void EpollServer::workerLoop() {
  epoll_event events[kMaxEvents];
  while (running_) {
    int n = epoll_wait(epfd_, events, kMaxEvents, 1000);
    if (n < 0) {
      if (errno == EINTR) continue;
      break;
    }
    for (int i = 0; i < n; ++i) {
      int fd = events[i].data.fd;
      if (fd == wakeFd_) {
        uint64_t v;
        while (::read(wakeFd_, &v, sizeof v) == sizeof v) {
        }
        std::deque<int> batch;
        {
          std::lock_guard<std::mutex> lk(pendingMtx_);
          batch.swap(pendingFds_);
        }
        for (int cfd : batch) {
          auto ctx = std::make_unique<ClientCtx>();
          ctx->ws = std::make_unique<WsConnection>([this, cfd](const std::string& t) {
            chat_.onMessage(cfd, t);
          });
          epoll_event ev{};
          ev.events = EPOLLIN;
          ev.data.fd = cfd;
          if (epoll_ctl(epfd_, EPOLL_CTL_ADD, cfd, &ev) < 0) {
            close(cfd);
            continue;
          }
          std::lock_guard<std::mutex> lk(connMtx_);
          conns_[cfd] = std::move(ctx);
        }
        continue;
      }

      uint32_t ev = events[i].events;
      if (ev & (EPOLLERR | EPOLLHUP)) {
        closeClient(fd);
        continue;
      }
      if (ev & EPOLLIN) {
        char buf[65536];
        ssize_t nr = ::read(fd, buf, sizeof buf);
        if (nr <= 0) {
          closeClient(fd);
          continue;
        }
        std::unique_lock<std::mutex> lk(connMtx_);
        auto it = conns_.find(fd);
        if (it == conns_.end()) {
          lk.unlock();
          continue;
        }
        WsConnection* ws = it->second->ws.get();
        lk.unlock();
        ws->appendRead(buf, static_cast<size_t>(nr));
        for (;;) {
          auto hs = ws->pollHandshakeResponse();
          if (!hs) break;
          enqueueRaw(fd, *hs);
        }
        if (ws->closed()) {
          closeClient(fd);
          continue;
        }
        flushOut(fd);
      }
      if (ev & EPOLLOUT) {
        flushOut(fd);
      }
    }
  }
}

bool EpollServer::sendWsJson(int fd, const nlohmann::json& envelope) {
  std::string payload = envelope.dump();
  auto frame = wsEncodeTextFrame(payload);
  enqueueRaw(fd, std::string(frame.begin(), frame.end()));
  flushOut(fd);
  return true;
}

void EpollServer::sendWsJsonToUser(int64_t uid, const nlohmann::json& envelope) {
  std::vector<int> fds;
  {
    std::lock_guard<std::mutex> lk(userMtx_);
    auto it = uidToFds_.find(uid);
    if (it != uidToFds_.end()) fds = it->second;
  }
  for (int fd : fds) sendWsJson(fd, envelope);
}

void EpollServer::registerUserConnection(int64_t uid, int fd) {
  std::lock_guard<std::mutex> lk(userMtx_);
  uidToFds_[uid].push_back(fd);
}

void EpollServer::unregisterUserConnection(int64_t uid, int fd) {
  std::lock_guard<std::mutex> lk(userMtx_);
  auto it = uidToFds_.find(uid);
  if (it == uidToFds_.end()) return;
  auto& v = it->second;
  v.erase(std::remove(v.begin(), v.end(), fd), v.end());
  if (v.empty()) uidToFds_.erase(it);
}

bool EpollServer::userHasConnection(int64_t uid) {
  std::lock_guard<std::mutex> lk(userMtx_);
  auto it = uidToFds_.find(uid);
  return it != uidToFds_.end() && !it->second.empty();
}
