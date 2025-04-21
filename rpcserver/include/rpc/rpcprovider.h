/**
 * @author EkerSun
 * @date 2025.3.28
 * @brief 发布远程服务调用
 */
#ifndef RPCPROVIDER_H
#define RPCPROVIDER_H
#include <unordered_map>
#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <google/protobuf/service.h>
#include "rpccontroller.h"
#include "mutex"

class RpcProvider
{
public:
    // 远程服务注册
    void NotifyService(std::unordered_map<std::string, google::protobuf::Service *>, std::unordered_set<std::string>);
    // 启动服务节点
    void Run();

private:
    ::muduo::net::EventLoop event_loop_;
    // 服务对象结构体
    struct ServiceInfo
    {
        // 服务对象
        google::protobuf::Service *service_;
        // 服务对象的方法
        std::unordered_map<std::string, const google::protobuf::MethodDescriptor *> method_map_;
    };
    // 存储注册成功的服务对象和其服务方法的所有信息
    std::unordered_map<std::string, ServiceInfo> service_map_;
    // 连接回调
    void OnConnection(const muduo::net::TcpConnectionPtr &);
    // 读写事件回调
    void OnMessage(const muduo::net::TcpConnectionPtr &, muduo::net::Buffer *, muduo::Timestamp);

    // Closure的回调操作，用于序列化响应和网络发送
    void SendRpcResponse(const muduo::net::TcpConnectionPtr &conn, google::protobuf::Message *response);
    // 长连接方法列表
    std::unordered_set<std::string> keep_alive_method_set_;
};
#endif
