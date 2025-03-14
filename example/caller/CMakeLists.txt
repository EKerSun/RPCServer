# 设置源文件
set(SRC_LIST calluserservice.cc ../user.pb.cc)
# 生成可执行文件consumer
add_executable(consumer ${SRC_LIST})
# 链接动态库rpcserver框架和protobuf
target_link_libraries(consumer rpcserver protobuf)