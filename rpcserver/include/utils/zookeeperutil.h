/**
 * @author EkerSun
 * @date 2025.3.28
 * @brief zookeeper客户端操作类
 */
#ifndef ZOOKEEPERUTIL_H
#define ZOOKEEPERUTIL_H
#include <string>
#include <zookeeper/zookeeper.h>
#include "rpcheader.pb.h"

class ZooKeeperClient
{
public:
    ZooKeeperClient();
    ~ZooKeeperClient();
    // 启动ZooKeeper客户端，连接ZooKeeper服务端
    void Start();
    // 在ZooKeeper服务端上根据指定的路径创建节点
    void Create(const std::string &path, std::string = "", int state = 0);
    // 根据参数指定的节点路径，获取节点的值
    bool GetData(const std::string &path, std::string &data);

private:
    // ZooKeeper客户端句柄
    zhandle_t *zhandle_;
};
#endif