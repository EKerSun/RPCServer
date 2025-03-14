/**
 * rpcserver的使用示例
 * 模拟服务提供端：provider
 */

#include <iostream>
#include <string>
#include "user.pb.h"
#include "rpcapplication.h"
#include "rpcprovider.h"

// 将UserService注册到Rpc框架中
class UserService : public userproto::UserServiceRpc
{
public:
    // 模拟本地登录服务
    bool Login(std::string name, std::string pwd)
    {
        // 输出正在执行本地服务：登录
        std::cout << "doing local service: Login" << std::endl;
        // 输出用户名和密码
        std::cout << "name:" << name << " pwd:" << pwd << std::endl;
        return false;
    }

    // 模拟本地注册服务
    bool Register(uint32_t id, std::string name, std::string pwd)
    {
        std::cout << "doing local service: Register" << std::endl;
        std::cout << "id:" << id << "name:" << name << " pwd:" << pwd << std::endl;
        return true;
    }

    /**
     * 重写基类UserServiceRpc的虚函数
     * controller:框架给业务上报的rpc上下文，包含了rpc请求的所有信息
     * request:客户端请求的参数
     * response:框架给业务上报的响应对象，业务通过该对象给客户端返回响应
     * done:回调操作
     */
    // 重写的登录方法
    void Login(::google::protobuf::RpcController *controller,
               const ::userproto::LoginRequest *request,
               ::userproto::LoginResponse *response,
               ::google::protobuf::Closure *done)
    {
        // 1. 框架给业务上报了请求参数LoginRequest，应用获取相应数据做本地业务
        std::string name = request->name();
        std::string pwd = request->pwd();

        // 2. 做本地业务
        bool login_result = Login(name, pwd);

        // 3. 把响应写入response, 写入格式由protobuf文件定义
        userproto::ResultCode *code = response->mutable_result();
        code->set_errcode(0);
        code->set_errmsg("Login Success!");
        response->set_sucess(login_result);
        // 执行回调操作，执行响应对象数据的序列化和网络发送（都是由框架来完成的）
        done->Run();
    }

    // 重写的注册方法
    void Register(::google::protobuf::RpcController *controller,
                  const ::userproto::RegisterRequest *request,
                  ::userproto::RegisterResponse *response,
                  ::google::protobuf::Closure *done)
    {
        // 1. 解析requset中的数据
        uint32_t id = request->id();
        std::string name = request->name();
        std::string pwd = request->pwd();
        // 2. 执行本地注册服务
        bool ret = Register(id, name, pwd);
        // 3. 写入response
        response->mutable_result()->set_errcode(0);
        response->mutable_result()->set_errmsg("");
        response->set_sucess(ret);
        // 4. 执行回调操作
        done->Run();
    }
};

int main(int argc, char **argv)
{
    // 调用框架的初始化操作
    RpcApplication::Init(argc, argv);

    // provider是一个rpc网络服务对象。把UserService对象发布到rpc节点上
    RpcProvider provider;
    provider.NotifyService(new UserService());

    // 启动一个rpc服务发布节点   Run以后，进程进入阻塞状态，等待远程的rpc调用请求
    provider.Run();

    return 0;
}