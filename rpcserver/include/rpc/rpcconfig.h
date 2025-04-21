/**
 * @author EkerSun
 * @date 2025.3.28
 * @brief 预处理配置文件
 */
#ifndef RPCCONFIG_H
#define RPCCONFIG_H

#include <unordered_map>
#include <unordered_set>
#include <string>

class RpcConfig
{
public:
    // 负责解析加载配置文件
    void LoadConfigFile(std::string &config_file);
    // 查询配置项信息
    std::string Load(const std::string &key);
    std::unordered_set<std::string> LoadService();

private:
    std::unordered_map<std::string, std::string> config_map_;
    std::unordered_set<std::string> service_set_;
    void Trim(std::string &src_buf);
    std::unordered_set<std::string> ParseToSet(std::string &s);
};
#endif