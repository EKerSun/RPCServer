#include "rpcprovider.h"
#include <google/protobuf/descriptor.h>
#include "asynclogger.h"
#include "rpcapplication.h"
#include "zookeeperutil.h"
#include "rpcheader.pb.h"
#include <unordered_set>

// 远程服务注册
void RpcProvider::NotifyService(std::unordered_map<std::string, google::protobuf::Service *> service_library,
                                std::unordered_set<std::string> keep_alive_method_set)
{
    // 当前节点配置的服务信息
    std::unordered_set<std::string> service_set = RpcApplication::GetInstance().GetConfig().LoadService(); // 存储服务名
    // std::unordered_set<std::string> method_set = RpcApplication::GetInstance().GetConfig().LoadService();   // 存储方法名

    for (const std::string &service_str : service_set)
    {
        auto it = service_library.find(service_str);
        if (it == service_library.end())
        {
            LOG_ERROR << "Service library: " << service_str << " is not exist! Please check the config file!";
            exit(EXIT_FAILURE);
        }

        ServiceInfo service_info;
        // 获取服务对象的描述信息
        const google::protobuf::ServiceDescriptor *p_service_desc = it->second->GetDescriptor();
        // 获取服务的名字
        std::string service_name = p_service_desc->name();
        // 获取服务对象的方法的数量
        int method_count = p_service_desc->method_count();

        LOG_INFO << "service name: " << service_name.c_str();
        for (int i = 0; i < method_count; ++i)
        {
            const google::protobuf::MethodDescriptor *p_method_desc = p_service_desc->method(i);
            std::string method_name = p_method_desc->name();
            service_info.method_map_.insert({method_name, p_method_desc});

            LOG_INFO << "method name: " << method_name.c_str();
        }
        service_info.service_ = it->second;
        service_map_.insert({service_name, service_info});
    }
    keep_alive_method_set_ = keep_alive_method_set;
}

// 启动服务节点
void RpcProvider::Run()
{
    // 读取配置文件
    std::string ip = RpcApplication::GetInstance().GetConfig().Load("rpcserverip");
    uint16_t port = atoi(RpcApplication::GetInstance().GetConfig().Load("rpcserverport").c_str());
    muduo::net::InetAddress address(ip, port);

    // 创建TcpServer对象
    muduo::net::TcpServer server(&event_loop_, address, "RpcProvider");

    // 绑定连接回调和消息读写回调方法
    server.setConnectionCallback(std::bind(&RpcProvider::OnConnection, this, std::placeholders::_1));
    server.setMessageCallback(std::bind(&RpcProvider::OnMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    // 设置muduo库的线程数量
    server.setThreadNum(4);

    // 把当前节点上要发布的服务全部注册到ZooKeeper上面
    ZooKeeperClient zookeeper_client;
    zookeeper_client.Start();
    for (auto &sp : service_map_)
    {
        std::string service_path = "/" + sp.first;
        // 创建服务节点
        zookeeper_client.Create(service_path);
        for (auto &mp : sp.second.method_map_)
        {
            std::string method_path = service_path + "/" + mp.first;
            std::string node_data = ip + ":" + std::to_string(port);
            // 创建方法节点
            zookeeper_client.Create(method_path, node_data);
        }
    }

    // rpc服务端准备启动，打印信息
    LOG_INFO << "RpcProvider start service at ip:" << ip << " port:" << port;

    // 启动网络服务
    server.start();
    event_loop_.loop();
}

// 连接回调
void RpcProvider::OnConnection(const muduo::net::TcpConnectionPtr &conn)
{
    if (!conn->connected())
    {
        conn->shutdown();
    }
}

// 读写事件回调
void RpcProvider::OnMessage(const muduo::net::TcpConnectionPtr &conn,
                            muduo::net::Buffer *buffer,
                            muduo::Timestamp)
{
    if (buffer->readableBytes() < 4)
    {
        return; // 数据不足，等待后续数据
    }
    const char *data = buffer->peek();
    uint32_t network_length = *(reinterpret_cast<const uint32_t *>(data));
    uint32_t length = ntohl(network_length); // 转换为本地字节序
    if (buffer->readableBytes() < 4 + length)
    {
        return; // 数据不足，等待后续数据
    }

    buffer->retrieve(4);
    std::string recv_buf = buffer->retrieveAllAsString();

    TheChat::RpcHeader rpc_header;
    if (!rpc_header.ParseFromString(recv_buf))
    {
        LOG_ERROR << "RPC header parse error!";
        return;
    }
    std::string service_name = rpc_header.service_name();
    std::string method_name = rpc_header.method_name();
    std::string params = rpc_header.params();

    auto sit = service_map_.find(service_name);
    if (sit == service_map_.end())
    {
        LOG_ERROR << service_name << " is not exist!";
        return;
    }
    auto mit = sit->second.method_map_.find(method_name);
    if (mit == sit->second.method_map_.end())
    {
        LOG_ERROR << service_name << ":" << method_name << " is not exist!";
        return;
    }
    google::protobuf::Service *service = sit->second.service_;
    const google::protobuf::MethodDescriptor *method = mit->second;

    // 生成RPC方法调用的请求和响应参数
    google::protobuf::Message *request = service->GetRequestPrototype(method).New();
    if (!request->ParseFromString(params))
    {
        LOG_ERROR << "request parse error!";
        return;
    }
    // service->GetDescriptor()->name();
    google::protobuf::Message *response = service->GetResponsePrototype(method).New();

    // 给RPC方法参数准备Closure的回调
    google::protobuf::Closure *done = google::protobuf::NewCallback<RpcProvider,
                                                                    const muduo::net::TcpConnectionPtr &,
                                                                    google::protobuf::Message *>(this,
                                                                                                 &RpcProvider::SendRpcResponse,
                                                                                                 conn, response);
    service->CallMethod(method, nullptr, request, response, done);
}

// Closure的回调操作，用于序列化响应和网络发送

void RpcProvider::SendRpcResponse(const muduo::net::TcpConnectionPtr &conn, google::protobuf::Message *response)
{
    std::string response_str;
    if (response->SerializeToString(&response_str)) // response进行序列化
    {
        // 序列化成功后，通过网络把rpc方法执行的结果发送会rpc的调用方
        conn->send(response_str);
    }
    else
    {
        std::cout << "serialize response_str error!" << std::endl;
    }
    conn->shutdown(); // 模拟http的短链接服务，由rpcprovider主动断开连接
}
