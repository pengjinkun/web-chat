#include "CryptoUtil.h"

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <iomanip>
#include <sstream>
#include <vector>

std::string md5Hex(const std::string& utf8) {
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int dlen = 0;
  EVP_MD_CTX* ctx = EVP_MD_CTX_new();
  if (!ctx) return {};
  const EVP_MD* md = EVP_md5();
  if (EVP_DigestInit_ex(ctx, md, nullptr) != 1 ||
      EVP_DigestUpdate(ctx, utf8.data(), utf8.size()) != 1 ||
      EVP_DigestFinal_ex(ctx, digest, &dlen) != 1) {
    EVP_MD_CTX_free(ctx);
    return {};
  }
  EVP_MD_CTX_free(ctx);
  std::ostringstream oss;
  for (unsigned i = 0; i < dlen; ++i) oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
  return oss.str();
}

std::string randomHex(size_t numBytes) {
  std::vector<unsigned char> buf(numBytes);
  if (RAND_bytes(buf.data(), static_cast<int>(numBytes)) != 1) return {};
  std::ostringstream oss;
  for (unsigned char c : buf) oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
  return oss.str();
}
