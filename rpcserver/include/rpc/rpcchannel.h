/**
 * @author EkerSun
 * @date 2025.3.28
 * @brief 用于服务发现和远程调用
 */
#ifndef RPCCHHNEL_H
#define RPCCHHNEL_H

#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>
#include "rpcexecption.h"
#include "circuitbreaker.h"
#include "zookeeperutil.h"
#include "connectionpool.h"
#include <mutex>
#include <queue>
#include <thread>
#include <atomic>
#include <condition_variable>

constexpr int CONNECT_TIMEOUT_MS = 1000;   // 连接超时时间
constexpr int SOCKET_RW_TIMEOUT_MS = 2000; // socket读写超时时间

class TheRpcChannel : public google::protobuf::RpcChannel
{
public:
    // 构造函数
    TheRpcChannel();
    // 析构函数
    ~TheRpcChannel();
    void CallMethod(const google::protobuf::MethodDescriptor *method,
                    google::protobuf::RpcController *controller,
                    const google::protobuf::Message *request,
                    google::protobuf::Message *response,
                    google::protobuf::Closure *done);

private:
    ZooKeeperClient zk_client_;
    int epoll_fd_;
    struct CachedEndpoint
    {
        Endpoint endpoint;                                 // 服务端点信息
        std::chrono::steady_clock::time_point expire_time; // 过期时间
        bool IsExpired() const
        {
            return std::chrono::steady_clock::now() > expire_time;
        }
    };

    void SendRequest(const ScopedFd &clientfd, const TheChat::RpcHeader &rpc_headers);
    void ReceiveResponse(const ScopedFd &clientfd, google::protobuf::Message *response, CircuitBreaker &breaker);
    constexpr bool ShouldTriggerCircuitBreak(RpcErrorType type);
    std::unordered_map<std::string, CircuitBreaker> breaker_map_;
    std::mutex breaker_mutex_;
    Endpoint GetServiceEndpoint(const std::string &service, const std::string &method);

};

#endif