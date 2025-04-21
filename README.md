# RPCServer
RPC分布式网络通信框架
## 功能
- 基于 Muduo 网络库实现 ​​Epoll ET 模式 + 非阻塞 I/O实现Reactor高并发远程服务提供方RpcProvider
- 设计基于 Protobuf 设计二进制通信协议​​（Header + Body），实现可靠和高效率通信和远程服务调用
- 设计连接池，使用分片机制减少锁竞争，基于非阻塞IO和epoll的连接健康检测，设计超时回收机制；设计连接复用和动态扩容机制，为RPC框架提供高效连接复用，避免频繁TCP握手/挥手开销，降低服务调用延迟
- 设计具有三种状态（正常/阻断/半开）的熔断器，在RPC框架中自动阻断异常服务流量，防止级联故障扩散，保障核心链路稳定性，通过半开状态试探性恢复，实现故障服务的无人工干预平滑恢复。
- 实现无锁的双缓冲异步日志系统，设计6个日志等级，提供多种刷盘策略和溢出策略，使用无锁设计减少竞争，支持日志滚动与微秒级时间戳
## 环境要求
- Ubuntu-24.04
- C++20
## 依赖
- muduo
- protobuf
- zookeeper
## 项目树
```bash
.
├── CMakeLists.txt
├── README.md
├── build
│   └── Makefile
├── lib
│   └── librpcserver.a
├── rpcheader.proto
└── rpcserver
    ├── CMakeLists.txt
    ├── include
    │   ├── execption
    │   │   ├── poolexecption.h
    │   │   └── rpcexecption.h
    │   ├── proxy
    │   │   ├── proxyserver.h
    │   │   └── proxyservice.h
    │   ├── rpc
    │   │   ├── rpcapplication.h
    │   │   ├── rpcchannel.h
    │   │   ├── rpcclosure.h
    │   │   ├── rpcconfig.h
    │   │   ├── rpccontroller.h
    │   │   ├── rpcheader.pb.h
    │   │   └── rpcprovider.h
    │   └── utils
    │       ├── asynclogger.h
    │       ├── circuitbreaker.h
    │       ├── connectionpool.h
    │       ├── doublebufferqueue.h
    │       ├── home
    │       ├── redis.hpp
    │       ├── scopedfd.h
    │       └── zookeeperutil.h
    └── src
        ├── CMakeLists.txt
        ├── proxy
        │   ├── proxyserver.cc
        │   └── proxyservice.cc
        ├── rpc
        │   ├── rpcapplication.cc
        │   ├── rpcchannel.cc
        │   ├── rpcconfig.cc
        │   ├── rpccontroller.cc
        │   ├── rpcheader.pb.cc
        │   └── rpcprovider.cc
        ├── rpcheader.proto
        └── utils
            ├── asynclogger.cc
            ├── circuitbreaker.cc
            ├── connectionpool.cc
            ├── redis.cpp
            ├── scopedfd.cc
            └── zookeeperutil.cc
```
## 简单使用
.....
编译项目生成库文件librpcserver.a
调用Provider提供服务
使用stub调用TheRPCchannel的callmethod方法，调用远程服务
后续提供简单的使用实例和接口说明
## 后续工作
- 完善controller和clousr的设计
- 实现异步CallMethod方法
- 深度集成熔断器和连接池
- 提供使用示例代码
- 提供框架图


