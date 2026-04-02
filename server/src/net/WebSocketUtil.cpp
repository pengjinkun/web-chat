#include "WebSocketUtil.h"

#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

#include <cstring>

static const char* WS_MAGIC = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

bool wsBuildAcceptKey(const std::string& secWebSocketKey, std::string& outAccept) {
  std::string combined = secWebSocketKey + WS_MAGIC;
  unsigned char sha[SHA_DIGEST_LENGTH];
  SHA1(reinterpret_cast<const unsigned char*>(combined.data()), combined.size(), sha);

  BIO* b64 = BIO_new(BIO_f_base64());
  BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
  BIO* bio = BIO_new(BIO_s_mem());
  bio = BIO_push(b64, bio);
  BIO_write(bio, sha, SHA_DIGEST_LENGTH);
  BIO_flush(bio);
  BUF_MEM* mem;
  BIO_get_mem_ptr(bio, &mem);
  outAccept.assign(mem->data, mem->length);
  BIO_free_all(bio);
  return true;
}

std::vector<uint8_t> wsEncodeTextFrame(const std::string& payload) {
  std::vector<uint8_t> out;
  out.push_back(0x81);
  size_t len = payload.size();
  if (len < 126) {
    out.push_back(static_cast<uint8_t>(len));
  } else if (len < 65536) {
    out.push_back(126);
    out.push_back(static_cast<uint8_t>((len >> 8) & 0xff));
    out.push_back(static_cast<uint8_t>(len & 0xff));
  } else {
    out.push_back(127);
    for (int i = 7; i >= 0; --i) out.push_back(static_cast<uint8_t>((len >> (i * 8)) & 0xff));
  }
  out.insert(out.end(), payload.begin(), payload.end());
  return out;
}

WsParseResult wsParseClientFrames(std::vector<uint8_t>& buf) {
  WsParseResult r;
  if (buf.size() < 2) return r;
  size_t i = 0;
  uint8_t b0 = buf[i++];
  uint8_t b1 = buf[i++];
  bool fin = (b0 & 0x80) != 0;
  uint8_t opcode = b0 & 0x0f;
  bool masked = (b1 & 0x80) != 0;
  uint64_t payloadLen = b1 & 0x7f;
  if (!masked) {
    r.ok = false;
    r.consumed = 0;
    return r;
  }
  if (payloadLen == 126) {
    if (buf.size() < i + 2) return r;
    payloadLen = (static_cast<uint64_t>(buf[i]) << 8) | buf[i + 1];
    i += 2;
  } else if (payloadLen == 127) {
    if (buf.size() < i + 8) return r;
    payloadLen = 0;
    for (int k = 0; k < 8; ++k) payloadLen = (payloadLen << 8) | buf[i + k];
    i += 8;
  }
  if (buf.size() < i + 4 + payloadLen) return r;
  uint8_t mask[4];
  std::memcpy(mask, buf.data() + i, 4);
  i += 4;
  std::string payload;
  payload.resize(static_cast<size_t>(payloadLen));
  for (uint64_t j = 0; j < payloadLen; ++j) {
    payload[j] = static_cast<char>(buf[i + j] ^ mask[j % 4]);
  }
  i += static_cast<size_t>(payloadLen);
  r.consumed = i;
  r.ok = fin;
  if (opcode == 0x8) {
    r.closed = true;
    r.ok = true;
    return r;
  }
  if (opcode == 0x1 && fin) {
    r.text = std::move(payload);
    return r;
  }
  if (opcode == 0x2 && fin) {
    r.text = std::move(payload);
    return r;
  }
  r.ok = false;
  r.consumed = i;
  return r;
}
