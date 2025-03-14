# RPCServer
RPC分布式网络通信框架
## 功能
- 基于muduo网络库实现RPC远程过程服务调用
- 使用protobuf数据序列化和反序列化协议
- 使用zookeeper实现分布式一致性协调服务
- 实现异步日志记录功能
## 环境要求
- Ubuntu-24.04
- C++17
## 依赖
- muduo
- protobuf
- zookeeper
# 项目树
```.
├── CMakeLists.txt
├── bin                       # 测试用例可执行文件和测试配置文件
│   ├── 2025-3-14-log.txt
│   ├── consumer
│   ├── provider
│   └── test.conf
├── build
│   └── Makefile
├── example                   # 测试用例
│   ├── CMakeLists.txt
│   ├── callee
│   │   ├── CMakeLists.txt
│   │   └── userservice.cc
│   ├── caller
│   │   ├── CMakeLists.txt
│   │   └── calluserservice.cc
│   ├── user.pb.cc
│   ├── user.pb.h
│   └── user.proto
├── lib                       # 框架静态库输出路径
│   └── librpcserver.a
└── src                       # 框架源文件
    ├── CMakeLists.txt
    ├── include               # 框架头文件
    │   ├── lockqueue.h
    │   ├── logger.h
    │   ├── rpcapplication.h
    │   ├── rpcchannel.h
    │   ├── rpcconfig.h
    │   ├── rpccontroller.h
    │   ├── rpcheader.pb.h
    │   ├── rpcprovider.h
    │   └── zookeeperutil.h
    ├── logger.cc
    ├── mprpcconfig.cc
    ├── rpcapplication.cc
    ├── rpcchannel.cc
    ├── rpcheader.pb.cc
    ├── rpcheader.proto
    ├── rpcprovider.cc
    └── zookeeperutil.cc
```





## 说明
本项目施个人学习项目，参考施磊老师的mprpc分布式网络通信框架项目
