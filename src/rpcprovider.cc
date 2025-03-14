#include "rpcprovider.h"
#include "rpcapplication.h"
#include "rpcheader.pb.h"
#include "logger.h"
#include "zookeeperutil.h"


/*
 * 这里是框架提供给外部使用的，可以发布rpc方法的函数接口
 * service: 要发布的服务对象，由外部业务层传入
 * 业务层的服务对象继承了基类google::protobuf::Service中的方法
 * 实现把外部传入的服务对象注册到框架(service_map_)中
*/
void RpcProvider::NotifyService(google::protobuf::Service *service)
{
    ServiceInfo service_info; // 服务信息结构体，包括服务类+存储方法的map

    // 获取了服务对象的描述信息
    const google::protobuf::ServiceDescriptor *p_service_desc = service->GetDescriptor();
    // 获取服务的名字
    std::string service_name = p_service_desc->name();
    // 获取服务对象service的方法的数量
    int method_count = p_service_desc->method_count();

    LOG_INFO("service_name:%s", service_name.c_str());

    for (int i=0; i < method_count; ++i)
    {
        // 获取了服务对象指定下标的服务方法的描述（抽象描述）
        const google::protobuf::MethodDescriptor* p_method_desc = p_service_desc->method(i);
        std::string method_name = p_method_desc->name();
        service_info.methon_map_.insert({method_name, p_method_desc});
        LOG_INFO("method_name:%s", method_name.c_str());
    }
    service_info.service_ = service;
    service_map_.insert({service_name, service_info});
}

/**
 * 启动rpc服务节点，开始提供rpc远程网络调用服务
 * 实现如下功能：
 * 1.读取配置文件rpcserver的信息,包括ip和port
 * 2.创建一个TcpServer对象，并设置相应的回调处理函数
 * 3.把当前rpc节点上要发布的服务全部注册到zookeeper上面,让client可以从zk上发现服务
 * 4.启动TcpServer开始监听客户端的rpc调用请求
 */
void RpcProvider::Run()
{
    // 读取配置文件rpcserver的信息
    std::string ip = RpcApplication::GetInstance().GetConfig().Load("rpcserverip");
    uint16_t port = atoi(RpcApplication::GetInstance().GetConfig().Load("rpcserverport").c_str());
    muduo::net::InetAddress address(ip, port);

    // 创建TcpServer对象
    muduo::net::TcpServer server(&eventLoop_, address, "RpcProvider");

    // 绑定连接回调和消息读写回调方法  分离了网络代码和业务代码
    server.setConnectionCallback(std::bind(&RpcProvider::OnConnection, this, std::placeholders::_1));
    server.setMessageCallback(std::bind(&RpcProvider::OnMessage, this, std::placeholders::_1, 
            std::placeholders::_2, std::placeholders::_3));
    // 设置muduo库的线程数量
    server.setThreadNum(4);

    // 把当前rpc节点上要发布的服务全部注册到zk上面，让rpc client可以从zk上发现服务
    ZkClient zookeeper_client;
    zookeeper_client.Start();
    // service_name为永久性节点    method_name为临时性节点
    for (auto &sp : service_map_) 
    {
        // service_path = "/service_name"
        std::string service_path = "/" + sp.first;
        zookeeper_client.Create(service_path.c_str(), nullptr, 0); // 创建永久性节点
        for (auto &mp : sp.second.methon_map_)
        {
            // method_path = "/service_name/method_name"  存储当前这个rpc服务节点主机的ip和port
            std::string method_path = service_path + "/" + mp.first;
            char method_path_data[128] = {0};
            sprintf(method_path_data, "%s:%d", ip.c_str(), port);
            // ZOO_EPHEMERAL表示znode是一个临时性节点
            zookeeper_client.Create(method_path.c_str(), method_path_data, strlen(method_path_data), ZOO_EPHEMERAL);
        }
    }
    // rpc服务端准备启动，打印信息
    std::cout << "RpcProvider start service at ip:" << ip << " port:" << port << std::endl;

    // 启动网络服务
    server.start();
    eventLoop_.loop(); 
}

// 新的socket连接回调
void RpcProvider::OnConnection(const muduo::net::TcpConnectionPtr &conn)
{
    if (!conn->connected())
    {
        // 和client的连接断开了
        conn->shutdown();
    }
}

/**
 * 在框架内部，RpcProvider和RpcConsumer协商好之间通信用的protobuf数据类型
 * 数据buffer: 包括三部分，数据头大小，数据头，参数体
 *      [0, 4]:header_size  
 *      [4, header_size + 4]: rpc_header, 包括 service_name method_name 参数体大小
 *      [header_size + 4,  buffer_size]:body
 */

// 已建立连接用户的读写事件回调 如果远程有一个rpc服务的调用请求，那么OnMessage方法就会响应
void RpcProvider::OnMessage(const muduo::net::TcpConnectionPtr &conn, 
                            muduo::net::Buffer *buffer, 
                            muduo::Timestamp)
{
    // 网络上接收的远程rpc调用请求的字符流
    std::string recv_buf = buffer->retrieveAllAsString();

    // 0:从第0个字节开始读取，4:读取4个字节，(char*)&header_size:将读取到的内容拷贝到header_size中
    uint32_t header_size = 0;
    recv_buf.copy((char*)&header_size, 4, 0);

    // 根据header_size读取数据头的原始字符流，反序列化数据，得到rpc请求的详细信息
    std::string rpc_header_str = recv_buf.substr(4, header_size);
    rpcserver::RpcHeader rpc_header;
    std::string service_name;
    std::string method_name;
    uint32_t args_size;
    if (rpc_header.ParseFromString(rpc_header_str))
    {
        // 数据头反序列化成功
        service_name = rpc_header.service_name();
        method_name = rpc_header.method_name();
        args_size = rpc_header.args_size();
    }
    else
    {
        // 数据头反序列化失败
        std::cout << "rpc_header_str:" << rpc_header_str << " parse error!" << std::endl;
        return;
    }

    // 获取rpc方法参数的字符流数据
    std::string args_str = recv_buf.substr(4 + header_size, args_size);

    // 获取service对象和method对象
    auto it = service_map_.find(service_name);
    if (it == service_map_.end())
    {
        std::cout << service_name << " is not exist!" << std::endl;
        return;
    }

    auto mit = it->second.methon_map_.find(method_name);
    if (mit == it->second.methon_map_.end())
    {
        std::cout << service_name << ":" << method_name << " is not exist!" << std::endl;
        return;
    }

    google::protobuf::Service *service = it->second.service_; // 获取service对象
    const google::protobuf::MethodDescriptor *method = mit->second; // 获取method对象

    // 生成rpc方法调用的请求request和响应response参数
    google::protobuf::Message *request = service->GetRequestPrototype(method).New();
    if (!request->ParseFromString(args_str))
    {
        std::cout << "request parse error, content:" << args_str << std::endl;
        return;
    }
    google::protobuf::Message *response = service->GetResponsePrototype(method).New();

    // 给下面的method方法的调用，绑定一个Closure的回调函数
    google::protobuf::Closure *done = google::protobuf::NewCallback<RpcProvider, 
                                                                    const muduo::net::TcpConnectionPtr&, 
                                                                    google::protobuf::Message*>
                                                                    (this, 
                                                                    &RpcProvider::SendRpcResponse, 
                                                                    conn, response);

    // 在框架上根据远端rpc请求，调用当前rpc节点上发布的方法
    service->CallMethod(method, nullptr, request, response, done);
}

// Closure的回调操作，用于序列化rpc的响应和网络发送
void RpcProvider::SendRpcResponse(const muduo::net::TcpConnectionPtr& conn, google::protobuf::Message *response)
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