cmake_minimum_required(VERSION 3.10)
project(RPC_DEMO)

# 设置c++标准
set(CMAKE_CXX_STANDARD 17)

# 设置所有可执行文件和库文件的输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build/lib) # 静态库目录
set(PROTOBUF_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/proto)

# 设置头文件路径
include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${PROTOBUF_INCLUDE_DIR}
)


# 1. 编译 myrpc 静态库
# 递归查找 src 目录下所有 .cpp 文件作为 myrpc 的源码
file(GLOB_RECURSE MYRPC_SOURCES ${PROJECT_SOURCE_DIR}/src/*.cpp)
add_library(myrpc STATIC ${MYRPC_SOURCES})

# 链接 myrpc 依赖的外部库(使用PRIVATE是因为这些依赖不暴露给myrpc用户)
target_link_libraries(myrpc PRIVATE pthread protobuf zookeeper_mt)

# 公共源码文件
file(GLOB SERVICE_SOURCES ${PROJECT_SOURCE_DIR}/service/*.cpp)
file(GLOB PROTOBUF_GENERATED_SOURCES ${PROJECT_SOURCE_DIR}/proto/*.cc)


# 2. 生成 rpc_server_demo 可执行文件
set(RPC_SERVER_MAIN ${PROJECT_SOURCE_DIR}/example/rpc_server_demo.cpp)
set(SERVER_SOURCES
    ${SERVICE_SOURCES}
    ${PROTOBUF_GENERATED_SOURCES}
    ${RPC_SERVER_MAIN}
)

add_executable(rpc_server_demo ${SERVER_SOURCES})

# 设置可执行文件的链接目标：
target_link_libraries(rpc_server_demo PRIVATE myrpc pthread protobuf zookeeper_mt)


# 3. 生成 rpc_client_demo 可执行文件
set(RPC_CLIENT_MAIN ${PROJECT_SOURCE_DIR}/example/rpc_client_demo.cpp)
set(CLIENT_SOURCES
    ${SERVICE_SOURCES}
    ${PROTOBUF_GENERATED_SOURCES}
    ${RPC_CLIENT_MAIN}
)

add_executable(rpc_client_demo ${CLIENT_SOURCES})

# 设置可执行文件的链接目标：依赖 myrpc 库，以及没有公开的公共库
target_link_libraries(rpc_client_demo PRIVATE myrpc pthread protobuf zookeeper_mt)