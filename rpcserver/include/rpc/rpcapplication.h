/**
 * @author EkerSun
 * @date 2025.3.28
 * @brief RPC应用程序类
 */
#ifndef RPCAPPLICATION_H
#define RPCAPPLICATION_H

#include "rpcconfig.h"
class RpcApplication
{
public:
    // 初始化应用程序
    static void Init(int argc, char **argv);
    // 获取应用程序实例
    static RpcApplication &GetInstance();
    // 获取应用程序配置
    static RpcConfig &GetConfig();

private:
    // 应用程序配置
    static RpcConfig config_;
    // 构造函数
    RpcApplication();
    // 禁止拷贝构造函数
    RpcApplication(const RpcApplication &) = delete;
    // 禁止移动构造函数
    RpcApplication(RpcApplication &&) = delete;
};

#endif
