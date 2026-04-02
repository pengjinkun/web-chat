#pragma once
#include <cstdio>
#include <cstring>
#include <ctime>

inline void logLine(const char* level, const char* msg) {
  std::time_t t = std::time(nullptr);
  char buf[32];
  std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", std::localtime(&t));
  std::fprintf(stderr, "[%s] [%s] %s\n", buf, level, msg);
}

inline void logErr(const char* msg) { logLine("ERROR", msg); }
inline void logInfo(const char* msg) { logLine("INFO", msg); }
