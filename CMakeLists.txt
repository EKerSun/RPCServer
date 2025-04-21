# 设置cmake的最低版本
cmake_minimum_required(VERSION 3.20)

# 设置项目名称
project(RPCServer)

# 设置C++编译版本
set(CMAKE_CXX_STANDARD 20)

# 生成debug版本，可以进行gdb调试
set(CMAKE_BUILD_TYPE "Debug")

# 设置项目可执行文件输出的路径
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

# 设置项目库文件输出的路径
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

# 设置项目编译头文件搜索路径
include_directories(${PROJECT_SOURCE_DIR}/rpcserver/include)
include_directories(${PROJECT_SOURCE_DIR}/rpcserver/include/utils)
include_directories(${PROJECT_SOURCE_DIR}/rpcserver/include/execption)
include_directories(${PROJECT_SOURCE_DIR}/rpcserver/include/proxy)
include_directories(${PROJECT_SOURCE_DIR}/rpcserver/include/rpc)
# 设置项目库文件搜索路径
link_directories(${PROJECT_SOURCE_DIR}/lib)

# src包含了rpcserver框架所有的相关代码
add_subdirectory(rpcserver)
