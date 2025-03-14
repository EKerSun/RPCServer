# 设置源文件
set(SRC_LIST userservice.cc ../user.pb.cc)
# 生成可执行文件provider
add_executable(provider ${SRC_LIST})
# 链接动态库rpcserver框架和protobuf
target_link_libraries(provider rpcserver protobuf)