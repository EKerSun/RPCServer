#include "rpcapplication.h"
#include <iostream>
#include <unistd.h>
#include <string>

RpcConfig RpcApplication::config_;

// 显示参数帮助信息
void ShowArgsHelp()
{
    // 输出命令格式
    std::cout<<"format: command -i <configfile>" << std::endl;
}

void RpcApplication::Init(int argc, char **argv)
{
    // 判断参数个数是否小于2，小于2则显示参数帮助并退出程序
    if (argc < 2)
    {
        ShowArgsHelp();
        exit(EXIT_FAILURE);
    }

    int c = 0;
    std::string config_file;
    // 使用getopt函数解析参数
    while((c = getopt(argc, argv, "i:")) != -1)
    {
        switch (c)
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
    // 开始加载配置文件了 rpcserver_ip=  rpcserver_port   zookeeper_ip=  zookepper_port=
    config_.LoadConfigFile(config_file.c_str());
}

// 获取RpcApplication类的实例
RpcApplication& RpcApplication::GetInstance()
{
    // 定义一个静态的RpcApplication对象
    static RpcApplication app;
    // 返回该对象
    return app;
}

// 获取RpcApplication的配置
RpcConfig& RpcApplication::GetConfig()
{
    // 返回config_成员变量
    return config_;
}