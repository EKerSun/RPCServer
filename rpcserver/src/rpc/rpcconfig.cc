#include "rpcconfig.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <filesystem>
#include "asynclogger.h"
// 负责解析加载配置文件
void RpcConfig::LoadConfigFile(std::string &config_file)
{
    if (!std::filesystem::exists(config_file))
    {
        LOG_ERROR << "Config file not found: " << config_file;
        exit(EXIT_FAILURE);
    }

    std::ifstream ifs(config_file);
    if (!ifs.is_open())
    {
        LOG_ERROR << "Failed to open config file: " << config_file;
        exit(EXIT_FAILURE);
    }
    std::string line;
    while (std::getline(ifs, line))
    {
        // 去掉注释和空行
        size_t comment_pos = line.find('#');
        if (comment_pos != std::string::npos)
        {
            line = line.substr(0, comment_pos); // 去掉注释部分
        }
        // 去掉首尾空格
        Trim(line);
        if (line.empty())
            continue;

        // 解析配置项
        size_t equal_pos = line.find('=');
        if (equal_pos == std::string::npos)
            continue;

        std::string key = line.substr(0, equal_pos);
        std::string value = line.substr(equal_pos + 1);
        Trim(key);
        Trim(value);
        if (key == "services")
        {
            service_set_.insert(value);
        }
        else
        {
            config_map_[key] = value;
        }
    }
    ifs.close();
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

std::unordered_set<std::string> RpcConfig::LoadService()
{
    // auto it = config_map_.find(key);
    // if (it == config_map_.end())
    // {
    //     return std::unordered_set<std::string>();
    // }
    // return RpcConfig::ParseToSet(it->second);
    return service_set_;
}

// 去掉字符串前后的空格
void RpcConfig::Trim(std::string &str)
{
    if (str.empty())
        return;
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
    {
        str.clear();
        return;
    }
    size_t end = str.find_last_not_of(" \t\r\n");
    str = str.substr(start, end - start + 1);
}

std::unordered_set<std::string> RpcConfig::ParseToSet(std::string &str)
{
    std::vector<std::string> elements;
    std::stringstream ss(str);
    std::string token;
    while (getline(ss, token, ','))
    {
        elements.push_back(token);
    }
    return std::unordered_set<std::string>(elements.begin(), elements.end());
}