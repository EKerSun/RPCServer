
#include "proxyservice.h"
#include <muduo/base/Logging.h>
#include <rpcheader.pb.h>
#include <redis.hpp>
#include <functional>

ConnectionManager &ConnectionManager::instance()
{
    static ConnectionManager mgr;
    return mgr;
}

// 注册连接并生成唯一ID
std::string ConnectionManager::registerConnection(const muduo::net::TcpConnectionPtr &conn, int client_id)
{
    std::string connId = boost::uuids::to_string(uuid_gen_());
    std::lock_guard<std::mutex> lock(mutex_);
    connections_[connId] = conn;
    connId_map_[client_id] = connId;
    return connId;
}

// 根据ID获取连接
muduo::net::TcpConnectionPtr ConnectionManager::getConnection(const std::string &connId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = connections_.find(connId);
    return (it != connections_.end()) ? it->second : nullptr;
}

// 根据client_id获取连接ID
std::string ConnectionManager::getConnId(int client_id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = connId_map_.find(client_id);
    return (it != connId_map_.end()) ? it->second : "";
}
// 删除连接
void ConnectionManager::removeConnection(int client_id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = connId_map_.find(client_id);
    connections_.erase(it->second);
    connId_map_.erase(client_id);
}

ProxyService::ProxyService()
{
    rpc_channel_ = new TheRpcChannel();
    local_id_ = boost::uuids::to_string(uuid_gen_());
}
ProxyService::~ProxyService()
{
    delete rpc_channel_;
}

void ProxyService::RpcProcess(const muduo::net::TcpConnectionPtr &conn,
                              muduo::net::Buffer *buffer,
                              muduo::Timestamp time)

{
    if (buffer->readableBytes() < sizeof(int))
    {
        LOG_INFO << "recv data length is less than 4";
        return;
    }
    const char *data = buffer->peek();
    uint32_t network_length = *(reinterpret_cast<const uint32_t *>(data));
    uint32_t length = ntohl(network_length);
    if (buffer->readableBytes() < 4 + length)
    {
        LOG_INFO << "recv data length is less than " << length;
        return;
    }
    buffer->retrieve(4);
    std::string recv_buf = buffer->retrieveAllAsString();

    TheChat::RequestHeader request_header;
    if (!request_header.ParseFromString(recv_buf))
    {
        LOG_INFO << "parse request header failed";
        return;
    }
    uint32_t message_id = request_header.message_id();
    MessageHandler msg_handler;
    if (!GetMessageHandler(message_id, msg_handler))
    {
        LOG_INFO << "message_id: " << message_id << " not found";
        return;
    }
    LOG_INFO << "message_id: " << message_id;
    msg_handler(request_header.content(), conn);
}

// 注册消息回调，添加新方法，覆盖旧方法
void ProxyService::RegisterMessageHandler(int message_id, MessageHandler &&cb)
{
    LOG_INFO << "RegisterMessageHandler: " << message_id;
    msg_handler_map_[message_id] = cb;
}

// 注册消息回调, 添加新方法，覆盖旧方法
void ProxyService::RegisterMessageHandler(std::unordered_map<int, MessageHandler> &msg_handler_map)
{
    for (auto &item : msg_handler_map)
    {
        msg_handler_map_[item.first] = item.second;
    }
}

void ProxyService::RegisterRedisSubHandler(RedisSubHandler &&cb)
{
    redis_sub_handler_ = cb;
    if (Redis::Instance().connect())
    {
        Redis::Instance().init_notify_handler(redis_sub_handler_);
        Redis::Instance().subscribe(local_id_);
    }
}

bool ProxyService::GetMessageHandler(uint32_t message_id, MessageHandler &message_handler)
{
    auto it = msg_handler_map_.find(message_id);
    if (it != msg_handler_map_.end())
    {
        message_handler = it->second;
        return true;
    }
    return false;
}
