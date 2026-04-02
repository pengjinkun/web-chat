#pragma once
#include <cstdint>

int setNonBlocking(int fd);
int createListenSocket(uint16_t port);
