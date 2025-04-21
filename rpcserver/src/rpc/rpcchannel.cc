#include "rpcchannel.h"
#include <string>
#include "rpcheader.pb.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include "rpcapplication.h"
#include "rpccontroller.h"
#include "zookeeperutil.h"
#include "circuitbreaker.h"
#include <fcntl.h>
#include "rpcexecption.h"
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <mutex>
#include "asynclogger.h"

TheRpcChannel::TheRpcChannel()
{
    zk_client_.Start();
}
TheRpcChannel::~TheRpcChannel()
{
}

/**
 * @brief rpc的rpcchannel类, 一个代理的所有stub都调用同一个TheRpcChannel对象
 * @param method 要远程调用的方法
 * @param controller_base rpc控制对象
 * @param request 请求参数，由客户端传入
 * @param response 响应参数，由远程服务端传入
 */
void TheRpcChannel::CallMethod(const google::protobuf::MethodDescriptor *method,
                               google::protobuf::RpcController *controller_base,
                               const google::protobuf::Message *request,
                               google::protobuf::Message *response,
                               google::protobuf::Closure *done)
{
    LOG_INFO << "CallMethod:";
    // 动态类型转换
    auto *controller = dynamic_cast<TheRpcController *>(controller_base);
    if (!controller)
    {
        throw std::runtime_error("Invalid controller type");
    }

    LOG_INFO << "获取服务名";
    // 获取服务名
    const std::string method_full_name = method->full_name();
    const auto &service_name = method->service()->name();

    LOG_INFO << "初始化熔断器";
    // 初始化熔断器
    auto &breaker = CircuitBreakerManager::GetInstance(service_name);

    LOG_INFO << "熔断检查";
    // 熔断检查（带快速失败路径）
    if (!breaker.AllowRequest())
    {
        controller->SetFailed("Service Unavailable: " + service_name);
        if (done)
            done->Run();
        return;
    }

    // 描述错误原因
    std::string error_reason;
    // 成功标志
    bool rpc_success = false;

    LOG_INFO << "try";
    try
    {
        // 准备请求头
        TheChat::RpcHeader rpc_header;
        rpc_header.set_service_name(service_name);
        rpc_header.set_method_name(method->name());
        LOG_INFO << "将请求参数序列化到请求头中";
        // 将请求参数序列化到请求头中
        if (!request->SerializeToString(rpc_header.mutable_params()))
        {
            throw RpcException("Serialize request failed");
        }
        LOG_INFO << "服务发现";
        // 服务发现
        Endpoint endpoint = GetServiceEndpoint(service_name, method->name());
        LOG_INFO << "建立连接";
        // 建立连接
        ScopedFd clientfd(ConnectionPool::GetInstance().Get(endpoint));
        LOG_INFO << "发送请求";
        // 发送请求
        SendRequest(clientfd, rpc_header);
        LOG_INFO << "接收响应（带熔断状态感知）";
        // 接收响应（带熔断状态感知）
        ReceiveResponse(clientfd, response, breaker);

        rpc_success = true;
        LOG_INFO << "rpc_success";
    }
    catch (const RpcException &e)
    {
        const RpcErrorType errorType = e.type();
        error_reason = e.what();

        if (errorType == RpcErrorType::UNAUTHORIZED)
        {
            controller->SetFailed("Unauthorized: " + method_full_name);
        }
        else
        {
            controller->SetFailed(error_reason.empty() ? "RPC failed: " + method_full_name : error_reason);
        }

        if (ShouldTriggerCircuitBreak(e.type()))
        {
            breaker.RecordFailure();
        }
    }
    catch (const std::exception &e)
    {
        breaker.RecordFailure();
        controller->SetFailed("System error: " + std::string(e.what()));
    }
    catch (...)
    {
        breaker.RecordFailure();
        controller->SetFailed("Unknown system error");
    }

    // 更新熔断器状态
    if (rpc_success)
    {
        // 成功则记录成功
        LOG_INFO << "RecordSuccess";
        breaker.RecordSuccess();
    }

    // 如果有回调，则执行异步回调
    if (done)
    {
        if (controller->IsCanceled())
        {
            done->Run(); // 处理取消回调
        }
        else
        {
            done->Run();
        }
    }
    LOG_INFO << "CallMethod end";
}

// 服务发现
Endpoint TheRpcChannel::GetServiceEndpoint(const std::string &service,
                                           const std::string &method)
{
    // 带TTL的本地缓存
    static std::mutex cache_mutex;
    static std::unordered_map<std::string, CachedEndpoint> endpoint_cache;

    const std::string cache_key = service + ":" + method;

    // 首先在缓存中查找
    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        auto it = endpoint_cache.find(cache_key);
        if (it != endpoint_cache.end() && !it->second.IsExpired())
        {
            return it->second.endpoint;
        }
    }
    // 在ZooKeeper中查找
    std::string path = "/" + service + "/" + method;
    std::string node_data;
    // 获取数据
    if (!zk_client_.GetData(path, node_data))
    {
        throw RpcException("Service unavailable: " + path, RpcErrorType::SERVICE_UNAVAILABLE);
    }
    if (node_data.empty())
    {
        throw RpcException("Service unavailable: " + path, RpcErrorType::SERVICE_UNAVAILABLE);
    }
    Endpoint endpoint;
    // 解析数据 node_data = ip:port
    int idx = node_data.find(":");
    if (idx == std::string::npos)
    {
        throw RpcException("Invalid node data: " + node_data, RpcErrorType::SERVICE_UNAVAILABLE);
    }
    std::string ip = node_data.substr(0, idx);
    uint16_t port = atoi(node_data.substr(idx + 1, node_data.size() - idx).c_str());
    endpoint = Endpoint{ip, port};
    // 设置5分钟的TTL缓存
    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        endpoint_cache[cache_key] = CachedEndpoint{
            endpoint,
            std::chrono::steady_clock::now() + std::chrono::minutes(5)};
    }
    return endpoint;
}

constexpr bool TheRpcChannel::ShouldTriggerCircuitBreak(RpcErrorType type)
{
    return type == RpcErrorType::NETWORK_ERROR ||
           type == RpcErrorType::TIMEOUT ||
           type == RpcErrorType::SERVICE_UNAVAILABLE;
}

// 发送请求（带超时重试机制）
void TheRpcChannel::SendRequest(const ScopedFd &clientfd, const TheChat::RpcHeader &rpc_header)
{

    std::string send_str;
    if (!rpc_header.SerializeToString(&send_str))
    {
        throw RpcException("Failed to serialize request header", RpcErrorType::PROTOCOL_ERROR);
    }

    // 在发送前添加长度头
    int data_length = send_str.size();
    uint32_t network_length = htonl(data_length);
    std::string header(reinterpret_cast<char *>(&network_length), 4);
    std::string total_send_data = header + send_str;

    if (-1 == send(clientfd, total_send_data.c_str(), total_send_data.size(), 0))
    {
        close(clientfd);
        throw RpcException("send() error: " + std::string(strerror(errno)), RpcErrorType::NETWORK_ERROR);
    }
}

// 接收响应（带熔断感知）
void TheRpcChannel::ReceiveResponse(const ScopedFd &clientfd,
                                    google::protobuf::Message *response,
                                    CircuitBreaker &breaker)
{
    LOG_INFO << "接收响应（带熔断感知）";
    char recv_buf[1024] = {0};
    int recv_size = 0;
    if (-1 == (recv_size = recv(clientfd, recv_buf, 1024, 0)))
    {
        close(clientfd);
        throw RpcException("recv() error: " + std::string(strerror(errno)), RpcErrorType::NETWORK_ERROR);
    }
    if (!response->ParseFromArray(recv_buf, recv_size))
    {
        close(clientfd);
        throw RpcException("Failed to parse response", RpcErrorType::INVALID_RESPONSE);
    }
    LOG_INFO << "接收响应（带熔断感知）";
}
