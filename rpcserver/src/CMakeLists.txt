# 设置源文件
aux_source_directory(. SRC_LIST)
aux_source_directory(./utils UTILS_LIST)
aux_source_directory(./proxy PROXY_LIST)
aux_source_directory(./rpc RPC_LIST)
# 必须添加 THREADED，该项是zookeeper的配置项，启用zookeeper.h中THREADED的内容
add_compile_definitions(THREADED)
# 生成静态库  
add_library(rpcserver ${SRC_LIST} ${UTILS_LIST} ${PROXY_LIST} ${RPC_LIST})
# 链接动态库
target_link_libraries(rpcserver muduo_net muduo_base pthread zookeeper_mt)