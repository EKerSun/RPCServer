#include "rpcapplication.h"
#include <iostream>
#include <unistd.h>
#include <string>

RpcConfig RpcApplication::config_;

// 显示参数帮助信息
void ShowArgsHelp()
{
    std::cerr << "format: command -i <configfile>" << std::endl;
}

void RpcApplication::Init(int argc, char **argv)
{
    // 判断参数个数是否小于2，小于2则显示参数帮助并退出程序
    if (argc < 2)
    {
        ShowArgsHelp();
        exit(EXIT_FAILURE);
    }

    int option = 0;
    std::string config_file;
    // 使用getopt函数解析参数
    while ((option = getopt(argc, argv, "i:")) != -1)
    {
        switch (option)
        {
        case 'i':
            // 如果参数为-i，则将optarg赋值给config_file
            config_file = optarg;
            break;
        case '?':
            // 如果参数错误，则显示参数帮助并退出程序
            ShowArgsHelp();
            exit(EXIT_FAILURE);
        case ':':
            // 如果参数缺少值，则显示参数帮助并退出程序
            ShowArgsHelp();
            exit(EXIT_FAILURE);
        default:
            break;
        }
    }
    //std::string config_file = "/home/szh/code/TheRPC/bin/test.conf";
    config_.LoadConfigFile(config_file);
}

// 获取RpcApplication类的实例
RpcApplication &RpcApplication::GetInstance()
{
    static RpcApplication app;
    return app;
}

// 获取RpcApplication的配置
RpcConfig &RpcApplication::GetConfig()
{
    // 返回config_成员变
    return config_;
}

// 构造函数
RpcApplication::RpcApplication() {}
