cmake_minimum_required(VERSION 3.0)
project(My-MPRPC)

# src目录下，框架的源码，会向外部提供一个so动态库，
# 生成debug版本，调试用
set(CMAKE_BUILD_TYPE "Debug")

# 项目可执行文件输出的路径
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
# 项目库文件输出的路径
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

# 项目编译头文件搜索路径
include_directories(${PROJECT_SOURCE_DIR}/src/include)
include_directories(${PROJECT_SOURCE_DIR}/example)
# 项目库文件搜索路径
link_directories(${PROJECT_SOURCE_DIR}/lib)

# 包含了mprpc框架下所有的代码
add_subdirectory(src)
# 包含了使用框架的示例代码
add_subdirectory(example)