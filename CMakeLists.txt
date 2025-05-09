cmake_minimum_required(VERSION 3.15)
project(BlockchainAudits LANGUAGES CXX)

# Require C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Threads for std::thread
find_package(Threads REQUIRED)

find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})
link_libraries(${OPENSSL_LIBRARIES})

# pkg-config for gRPC and Protobuf
find_package(PkgConfig REQUIRED)
pkg_check_modules(GRPC    REQUIRED grpc++)
pkg_check_modules(PROTOBUF REQUIRED protobuf)

# Include directories
include_directories(
  ${GRPC_INCLUDE_DIRS}
  ${PROTOBUF_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}/proto
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR}/generated
)

# Link directories for pkg-config libraries
link_directories(
  ${GRPC_LIBRARY_DIRS}
  ${PROTOBUF_LIBRARY_DIRS}
)

# Collect generated protobuf sources
file(GLOB GENERATED_SRC
  "${CMAKE_CURRENT_BINARY_DIR}/generated/*.pb.cc"
  "${CMAKE_CURRENT_BINARY_DIR}/generated/*.grpc.pb.cc"
)

# Collect server sources
file(GLOB SERVER_SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/server.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/config_loader.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/mempool_manager.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/merkle_tree.cpp"
)

# Collect client sources
file(GLOB CLIENT_SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/client.cpp"
)

# Build the node server
add_executable(node_server
  ${SERVER_SRCS}
  ${GENERATED_SRC}
)
target_link_libraries(node_server
  PRIVATE
    ${GRPC_LIBRARIES}
    ${PROTOBUF_LIBRARIES}
    Threads::Threads
)

# Build the smoke-test client
add_executable(client
  ${CLIENT_SRCS}
  ${GENERATED_SRC}
)
target_link_libraries(client
  PRIVATE
    ${GRPC_LIBRARIES}
    ${PROTOBUF_LIBRARIES}
    Threads::Threads
)