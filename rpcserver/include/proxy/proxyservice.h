/**
 * @brief 代理服务，通过解析的请求，调用zookeeper获取服务端地址，并调用远程服务
 * @author EkerSun
 * @date 2025-4-15
 */

#ifndef PROXYSERVICE_H
#define PROXYSERVICE_H
#include <functional>
#include <unordered_map>

#include "rpccontroller.h"
#include "rpcchannel.h"
#include "zookeeperutil.h"

#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>

class ConnectionManager
{
public:
    static ConnectionManager &instance();
    // 注册连接并生成唯一ID
    std::string registerConnection(const muduo::net::TcpConnectionPtr &conn, int client_id);
    // 根据ID获取连接
    muduo::net::TcpConnectionPtr getConnection(const std::string &connId);
    // 根据client_id获取连接ID
    std::string getConnId(int client_id);
    // 删除连接
    void removeConnection(int client_id);

private:
    std::mutex mutex_;
    boost::uuids::random_generator uuid_gen_;
    std::unordered_map<std::string, muduo::net::TcpConnectionPtr> connections_;
    std::unordered_map<int, std::string> connId_map_;
};

// 作为客户端代理，通过RPC调用远程服务
using MessageHandler = std::function<void(const std::string &, const muduo::net::TcpConnectionPtr &)>;
using RedisSubHandler = std::function<void(std::string, std::string)>;

class ProxyService
{
public:
    ProxyService();
    ~ProxyService();

    void RpcProcess(const muduo::net::TcpConnectionPtr &conn,
                    muduo::net::Buffer *buffer,
                    muduo::Timestamp time);

    void RegisterMessageHandler(int message_id, MessageHandler &&cb);
    void RegisterMessageHandler(std::unordered_map<int, MessageHandler> &msg_handler_map);
    void RegisterRedisSubHandler(RedisSubHandler &&cb);

private:
    // 存储所有的rpc服务调用方法
    std::unordered_map<int, MessageHandler> msg_handler_map_;
    // RPC控制器
    TheRpcController *rpc_controller_;
    // 存储所有在线用户的连接
    std::unordered_map<int, muduo::net::TcpConnectionPtr> user_connection_map_;
    // 本代理的id
    std::string local_id_;
    // 随机生成器
    boost::uuids::random_generator uuid_gen_;
    // 远程服务调用通道
    TheRpcChannel *rpc_channel_;
    // 获取消息处理函数
    bool GetMessageHandler(uint32_t message_id, MessageHandler &msg_handler);
    
    RedisSubHandler redis_sub_handler_;


};

#endif
