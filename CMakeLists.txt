

cmake_minimum_required(VERSION 3.10)
project(RPC_DEMO)

# 设置c++标准
set(CMAKE_CXX_STANDARD 17)

set(PROTOBUF_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/proto)

# 设置头文件路径
include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${PROTOBUF_INCLUDE_DIR}
)


# file(GLOB_RECURSE SRC_SOURSE ${PROJECT_SOURCE_DIR}/src/*.cpp)
# # 指定生成库文件的路径
# set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/build/bin)
# # 生成静态库文件
# add_library(myrpc STATIC ${SRC_SOURSE})
# # 连接动态库
# target_link_libraries(myrpc pthread protobuf zookeeper_mt)


# 使用myrpc静态库生成rpc_server_demo
# set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/build/bin)
# file(GLOB CALCULATOR_SERVICE ${PROJECT_SOURCE_DIR}/service/*.cpp)
# file(GLOB PROTOBUF_DIR ${PROJECT_SOURCE_DIR}/proto/*.cc)
# set(RPC_SERVER_DEMO ${PROJECT_SOURCE_DIR}/example/rpc_server_demo.cpp)
# set(SOURSE ${CALCULATOR_SERVICE} ${RPC_SERVER_DEMO} ${PROTOBUF_DIR})
# link_libraries(myrpc pthread protobuf zookeeper_mt)
# link_directories(${PROJECT_SOURCE_DIR}/build/bin)
# add_executable(rpc_server_demo ${SOURSE})


# 使用myrpc静态库生成rpc_client_demo
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/build/bin)
file(GLOB CALCULATOR_SERVICE ${PROJECT_SOURCE_DIR}/service/*.cpp)
file(GLOB PROTOBUF_DIR ${PROJECT_SOURCE_DIR}/proto/*.cc)
set(RPC_SERVER_DEMO ${PROJECT_SOURCE_DIR}/example/rpc_client_demo.cpp)
set(SOURSE ${CALCULATOR_SERVICE} ${RPC_SERVER_DEMO} ${PROTOBUF_DIR})
link_libraries(myrpc pthread protobuf zookeeper_mt)
link_directories(${PROJECT_SOURCE_DIR}/build/bin)
add_executable(rpc_client_demo ${SOURSE})








# 设置源文件
# service.cpp 的路径
# main函数路径
# set(RPC_SERVER_DEMO ${PROJECT_SOURCE_DIR}/example/rpc_client_demo.cpp)
# protoc 编译后的文件路径
# file(GLOB PROTOBUF_DIR ${PROJECT_SOURCE_DIR}/proto/*.cc)

# 指定可执行程序输出路径(当前生成的是静态库，而不是可执行程序)
# set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/build/bin)

# 连接静态库
# link_libraries(myrpc pthread protobuf zookeeper_mt)
# link_directories(${PROJECT_SOURCE_DIR}/build/bin)

# 生成可执行文件(必须要有main)
# add_executable(rpc_client_demo ${SOURSE})

# 指定生成库文件的路径
# set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/build/bin)

# 生成静态库文件
# add_library(myrpc STATIC ${SRC_SOURSE})

# 连接动态库
# target_link_libraries(myrpc pthread protobuf zookeeper_mt)