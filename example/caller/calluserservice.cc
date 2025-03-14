/**
 * rpcserver的使用示例
 * 模拟服务提使用端：provider
 */
#include <iostream>
#include "rpcapplication.h"
#include "user.pb.h"
#include "rpcchannel.h"

int main(int argc, char **argv)
{
    // 初始化RpcApplication
    RpcApplication::Init(argc, argv);
    userproto::UserServiceRpc_Stub stub(new TheRpcChannel());

    // *** 演示调用远程发布的rpc方法Login *** //
    // 1. rpc方法的请求参数
    userproto::LoginRequest request;
    request.set_name("zhang san");
    request.set_pwd("123456");

    // 2. rpc方法的响应
    userproto::LoginResponse response;

    // 3. 发起rpc调用请求，等待返回结果
    stub.Login(nullptr, &request, &response, nullptr);

    // 4. 一次rpc调用完成，读调用的结果
    if (0 == response.result().errcode())
    {
        std::cout << "rpc login response success:" << response.sucess() << std::endl;
    }
    else
    {
        std::cout << "rpc login response error : " << response.result().errmsg() << std::endl;
    }

    // *** 演示调用远程发布的rpc方法Register *** //
    // 1. rpc方法的请求参数
    userproto::RegisterRequest req;
    req.set_id(2000);
    req.set_name("mprpc");
    req.set_pwd("666666");
    // 2. rpc方法的响应
    userproto::RegisterResponse rsp;

    // 3. 发起rpc调用请求，等待返回结果
    stub.Register(nullptr, &req, &rsp, nullptr); 

    // 4. 一次rpc调用完成，读调用的结果
    if (0 == rsp.result().errcode())
    {
        std::cout << "rpc register response success:" << rsp.sucess() << std::endl;
    }
    else
    {
        std::cout << "rpc register response error : " << rsp.result().errmsg() << std::endl;
    }
    
    return 0;
}