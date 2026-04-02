#pragma once

#include <cstdint>
#include <string>
#include <vector>

bool wsBuildAcceptKey(const std::string& secWebSocketKey, std::string& outAccept);

std::vector<uint8_t> wsEncodeTextFrame(const std::string& payload);

struct WsParseResult {
  bool ok{false};
  bool closed{false};
  std::string text;
  size_t consumed{0};
};

WsParseResult wsParseClientFrames(std::vector<uint8_t>& buf);
