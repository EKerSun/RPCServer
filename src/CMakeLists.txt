aux_source_directory(. SRC_LIST)
# 必须添加 THREADED
add_compile_definitions(THREADED)
# 生成静态库  
add_library(RPCServer ${SRC_LIST})
# 链接动态库
target_link_libraries(RPCServer muduo_net muduo_base pthread zookeeper_mt)