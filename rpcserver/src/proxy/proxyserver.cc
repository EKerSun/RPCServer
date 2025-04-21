#include "proxyserver.h"
#include <muduo/net/TcpServer.h>
#include "asynclogger.h"
#include "rpcheader.pb.h"
#include "rpcapplication.h"

void ProxyServer::Start()
{
    // 读取配置文件
    std::string ip = RpcApplication::GetInstance().GetConfig().Load("gateserverip");
    uint16_t port = atoi(RpcApplication::GetInstance().GetConfig().Load("gateserverport").c_str());
    muduo::net::InetAddress address(ip, port);
    // 创建TcpServer对象
    muduo::net::TcpServer server(&loop_, address, "ProxyServer");
    // 设置链接回调
    server.setConnectionCallback(std::bind(&ProxyServer::onConnection, this, std::placeholders::_1));
    // 设置消息回调
    server.setMessageCallback(std::bind(&ProxyServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    // 设置线程数量
    server.setThreadNum(4);
    server.start();
    loop_.loop();
}

void ProxyServer::RegisterMessageHandler(int message_id, MessageHandler &&cb)
{
    proxy_service_.RegisterMessageHandler(message_id, std::move(cb));
}

// 上报链接相关信息的回调函数
void ProxyServer::onConnection(const muduo::net::TcpConnectionPtr &conn)
{
    if (!conn->connected())
    {
        conn->shutdown();
    }
}

// 上报读写事件相关的回调函数
void ProxyServer::onMessage(const muduo::net::TcpConnectionPtr &conn,
                            muduo::net::Buffer *buffer,
                            muduo::Timestamp time)

{
    LOG_INFO << "ProxyServer::onMessage";
    proxy_service_.RpcProcess(conn, buffer, time);
}
