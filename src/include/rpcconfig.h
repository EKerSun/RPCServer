#ifndef RPCCONFIG_H
#define RPCCONFIG_H
#include <unordered_map>
#include <string>
/**
 * 配置文件格式
 * rpcserverip=127.0.0.1
 * rpcserverport=12345
 * zookeeperip=127.0.0.1
 * zookeeperport=2181
 */
// 框架读取配置文件类
class RpcConfig
{
public:
    // 负责解析加载配置文件
    void LoadConfigFile(const char *config_file);
    // 查询配置项信息
    std::string Load(const std::string &key);

private:
    std::unordered_map<std::string, std::string> config_map_;
    // 去掉字符串前后的空格
    void Trim(std::string &src_buf);
};
#endif