# CMakeLists.txt

cmake_minimum_required(VERSION 3.15)
project(BlockchainAudits LANGUAGES CXX)

# Require C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Threads
find_package(Threads REQUIRED)

# OpenSSL for signing/verification
find_package(OpenSSL REQUIRED)

# gRPC & Protobuf via pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(GRPC    REQUIRED grpc++)
pkg_check_modules(PROTOBUF REQUIRED protobuf)

# JSON (header-only, ordered_json)
find_package(nlohmann_json 3.2.0 REQUIRED)

# Include dirs
include_directories(
  ${GRPC_INCLUDE_DIRS}
  ${PROTOBUF_INCLUDE_DIRS}
  ${OPENSSL_INCLUDE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/proto
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR}/generated
)

# Link dirs
link_directories(
  ${GRPC_LIBRARY_DIRS}
  ${PROTOBUF_LIBRARY_DIRS}
)

# Generated Proto sources
file(GLOB GENERATED_SRC
  "${CMAKE_CURRENT_BINARY_DIR}/generated/*.pb.cc"
  "${CMAKE_CURRENT_BINARY_DIR}/generated/*.grpc.pb.cc"
)

# Server sources
file(GLOB SERVER_SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/server.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/config_loader.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/mempool_manager.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/merkle_tree.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/chain_manager.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/leader_config.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/block_scheduler.cpp"
)

# Client sources
file(GLOB CLIENT_SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/client.cpp"
)

# Node server target
add_executable(node_server
  ${SERVER_SRCS}
  ${GENERATED_SRC}
)
target_link_libraries(node_server
  PRIVATE
    ${GRPC_LIBRARIES}
    ${PROTOBUF_LIBRARIES}
    Threads::Threads
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
)

# Smoke-test client target
add_executable(client
  ${CLIENT_SRCS}
  ${GENERATED_SRC}
)
target_link_libraries(client
  PRIVATE
    ${GRPC_LIBRARIES}
    ${PROTOBUF_LIBRARIES}
    Threads::Threads
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
)

# Generate test files as well
add_executable(test_chain_manager
  tests/test_chain_manager.cpp
  src/chain_manager.cpp
)
target_include_directories(test_chain_manager PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR}/generated
)
target_link_libraries(test_chain_manager
  PRIVATE
    Threads::Threads
    nlohmann_json::nlohmann_json
)