#include "WsConnection.h"
#include "WebSocketUtil.h"

#include <cctype>
#include <sstream>
#include <string>

static void trim(std::string& s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(s.begin());
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();
}

static bool extractSecWebSocketKey(const std::string& req, std::string& out) {
  std::istringstream iss(req);
  std::string line;
  const std::string needle = "sec-websocket-key:";
  while (std::getline(iss, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    std::string low = line;
    for (char& c : low)
      if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    if (low.size() >= needle.size() && low.compare(0, needle.size(), needle) == 0) {
      size_t colon = line.find(':');
      if (colon == std::string::npos) return false;
      out = line.substr(colon + 1);
      trim(out);
      return !out.empty();
    }
  }
  return false;
}

void WsConnection::appendRead(const char* data, size_t n) {
  if (closed_) return;
  in_.insert(in_.end(), data, data + n);
  if (state_ == WsState::Handshake) {
    tryHandshake();
  } else {
    processFrames();
  }
}

bool WsConnection::tryHandshake() {
  std::string raw(in_.begin(), in_.end());
  size_t end = raw.find("\r\n\r\n");
  if (end == std::string::npos) return false;
  std::string req = raw.substr(0, end);
  std::string key;
  if (!extractSecWebSocketKey(req, key)) {
    closed_ = true;
    return false;
  }
  std::string accept;
  wsBuildAcceptKey(key, accept);
  std::ostringstream oss;
  oss << "HTTP/1.1 101 Switching Protocols\r\n"
      << "Upgrade: websocket\r\n"
      << "Connection: Upgrade\r\n"
      << "Sec-WebSocket-Accept: " << accept << "\r\n\r\n";
  handshakeResponse_ = oss.str();
  in_.erase(in_.begin(), in_.begin() + static_cast<std::ptrdiff_t>(end + 4));
  state_ = WsState::Open;
  processFrames();
  return true;
}

std::optional<std::string> WsConnection::pollHandshakeResponse() {
  if (handshakeResponse_.empty()) return std::nullopt;
  std::string s = std::move(handshakeResponse_);
  handshakeResponse_.clear();
  return s;
}

void WsConnection::processFrames() {
  while (state_ == WsState::Open && !in_.empty()) {
    WsParseResult pr = wsParseClientFrames(in_);
    if (pr.closed) {
      closed_ = true;
      in_.clear();
      return;
    }
    if (pr.consumed == 0) return;
    in_.erase(in_.begin(), in_.begin() + static_cast<std::ptrdiff_t>(pr.consumed));
    if (pr.ok && !pr.text.empty() && onText_) onText_(pr.text);
  }
}
