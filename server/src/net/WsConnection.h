#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

enum class WsState { Handshake, Open };

class WsConnection {
 public:
  using OnText = std::function<void(const std::string&)>;

  explicit WsConnection(OnText onText) : onText_(std::move(onText)) {}

  void appendRead(const char* data, size_t n);
  std::optional<std::string> pollHandshakeResponse();
  bool ready() const { return state_ == WsState::Open; }
  bool closed() const { return closed_; }

 private:
  bool tryHandshake();
  void processFrames();

  OnText onText_;
  WsState state_{WsState::Handshake};
  std::vector<uint8_t> in_;
  std::string handshakeResponse_;
  bool closed_{false};
};
