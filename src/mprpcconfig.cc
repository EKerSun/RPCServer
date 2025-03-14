#include "rpcconfig.h"
#include <iostream>
#include <string>

// 负责解析加载配置文件
void RpcConfig::LoadConfigFile(const char *config_file)
{
    // 打开配置文件
    FILE *pf = fopen(config_file, "r");
    if (nullptr == pf)
    {
        std::cout << config_file << " is note exist!" << std::endl;
        exit(EXIT_FAILURE);
    }

    while (!feof(pf))
    {
        char buf[512] = {0};
        fgets(buf, 512, pf);
        std::string read_buf(buf);

        // 去掉注释和空行
        if (read_buf[0] == '#' || read_buf.empty())
        {
            continue;
        }
        // 去掉字符串前面多余的空格
        Trim(read_buf);

        // 解析配置项
        int idx = read_buf.find('=');
        if (idx == -1)
        {
            continue;
        }

        std::string key;
        std::string value;
        // 获取配置项的键
        key = read_buf.substr(0, idx);
        Trim(key);
        int endidx = read_buf.find('\n', idx);
        // 获取配置项的值
        value = read_buf.substr(idx + 1, endidx - idx - 1);
        Trim(value);
        // 将配置项插入到map中
        config_map_.insert({key, value});
    }
    // 关闭配置文件
    fclose(pf);
}

// 查询配置项信息
std::string RpcConfig::Load(const std::string &key)
{
    auto it = config_map_.find(key);
    if (it == config_map_.end())
    {
        return "";
    }
    return it->second;
}

// 去掉字符串前后的空格
void RpcConfig::Trim(std::string &src_buf)
{
    int idx = src_buf.find_first_not_of(' ');
    if (idx != -1)
    {
        // 说明字符串前面有空格
        src_buf = src_buf.substr(idx, src_buf.size() - idx);
    }
    // 去掉字符串后面多余的空格
    idx = src_buf.find_last_not_of(' ');
    if (idx != -1)
    {
        // 说明字符串后面有空格
        src_buf = src_buf.substr(0, idx + 1);
    }
}