# ifndef RPCAPPLICATION_H
# define RPCAPPLICATION_H
#include "rpcconfig.h"

// rpc框架的基础类，负责框架的一些初始化操作
class RpcApplication
{
public:
    // 初始化函数，用于初始化应用程序
    static void Init(int argc, char **argv);
    // 获取应用程序实例
    static RpcApplication &GetInstance();
    // 获取应用程序配置
    static RpcConfig &GetConfig();

private:
    // 应用程序配置
    static RpcConfig config_;

    // 构造函数
    RpcApplication() {}
    // 禁止拷贝构造函数
    RpcApplication(const RpcApplication &) = delete;
    // 禁止移动构造函数
    RpcApplication(RpcApplication &&) = delete;
};

# endif