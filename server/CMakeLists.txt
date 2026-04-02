cmake_minimum_required(VERSION 3.16)
project(web_chat_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT UNIX)
  message(FATAL_ERROR "This server targets Linux (Epoll). Use WSL or a Linux host to build and run.")
endif()

find_package(OpenSSL 1.1.0 REQUIRED)
find_package(Threads REQUIRED)

find_path(MYSQL_INCLUDE_DIR mysql/mysql.h PATHS /usr/include/mysql /usr/local/include/mysql /usr/include/mariadb)
find_library(MYSQLCLIENT_LIB NAMES mysqlclient mariadb PATHS /usr/lib/x86_64-linux-gnu /usr/lib64 /usr/local/lib)

if(NOT MYSQL_INCLUDE_DIR OR NOT MYSQLCLIENT_LIB)
  message(FATAL_ERROR "libmysqlclient not found. Install: sudo apt install libmysqlclient-dev (or default-libmysqlclient-dev)")
endif()

find_library(HIREDIS_LIB hiredis PATHS /usr/lib/x86_64-linux-gnu /usr/lib64 /usr/local/lib)
if(NOT HIREDIS_LIB)
  message(FATAL_ERROR "hiredis not found. Install: sudo apt install libhiredis-dev")
endif()

include(FetchContent)
FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

add_executable(web_chat_server
  src/main.cpp
  src/net/SocketUtil.cpp
  src/net/EpollServer.cpp
  src/net/WsConnection.cpp
  src/net/WebSocketUtil.cpp
  src/util/CryptoUtil.cpp
  src/db/MysqlStore.cpp
  src/redis/RedisStore.cpp
  src/chat/ChatHandler.cpp
)

target_include_directories(web_chat_server PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${MYSQL_INCLUDE_DIR}
)

target_link_libraries(web_chat_server PRIVATE
  OpenSSL::Crypto
  ${MYSQLCLIENT_LIB}
  ${HIREDIS_LIB}
  Threads::Threads
  nlohmann_json::nlohmann_json
)

target_compile_options(web_chat_server PRIVATE -Wall -Wextra -Wno-unused-parameter)
