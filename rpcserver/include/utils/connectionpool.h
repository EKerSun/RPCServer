#ifndef CONNECTIONPOOL_H
#define CONNECTIONPOOL_H

#include <atomic>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include "rpcheader.pb.h"
#include <thread>
#include <unistd.h>
#include "scopedfd.h"

// 节点信息
struct Endpoint
{
    // 地址和端口
    std::string host;
    int port;

    // 支持比较
    bool operator==(const Endpoint &other) const
    {
        return host == other.host && port == other.port;
    }
};

// 节点信息哈希
namespace std
{
    template <>
    struct hash<Endpoint>
    {
        size_t operator()(const Endpoint &ep) const
        {
            return hash<string>{}(ep.host) ^ (hash<int>{}(ep.port) << 1);
        }
    };
}

// 连接池
class ConnectionPool
{
    // 节点信息
    struct PoolShard
    {
        std::mutex mutex;
        std::condition_variable cv;
        std::queue<int> idle_fds; // 空闲连接队列
        size_t active_count = 0;  // 活跃连接数
        time_t last_active = 0;   // 最后活动时间
    };
    // 全局状态
    struct GlobalState
    {
        std::atomic<size_t> total_conn{0}; // 当前连接数
        std::atomic<size_t> waiters{0};    // 等待连接数
        int epoll_fd = -1;                 // 全局epoll
        std::atomic_bool running{true};    // 全局运行状态
    };

public:
    /**
     * @brief 构造函数，实现初始化epoll和启动清理线程
     * @param max_conn 最大连接数
     * @param shard_num 分片数
     * @param idle_timeout 空闲超时时间
     */
    explicit ConnectionPool(size_t max_conn = 1024,
                            size_t shard_num = 16,
                            time_t idle_timeout = 30);

    /**
     * @brief 析构函数，实现停止清理线程和关闭全局epoll
     */
    ~ConnectionPool();

    // 获取单例
    static ConnectionPool &GetInstance();

    /**
     * @brief 获取一个连接，如果超时则抛出异常
     * @param ep 端点信息，包括host+port
     * @param timeout_ms 超时时间，单位毫秒，0表示不超时
     * @return 连接文件描述符
     */
    int Get(const Endpoint &ep, time_t timeout_ms = 3000);

    /**
     * @brief 释放一个连接，如果连接无效则关闭
     * @param fd 连接文件描述符
     * @param ep 端点信息，包括host+port
     */
    void Release(int fd, const Endpoint &ep) noexcept;

private:
    // 获取分片
    PoolShard &GetShard(const Endpoint &ep);

    // 检查连接是否有效
    bool Validate(int fd) noexcept;
    /**
     * @brief 创建新连接，如果超时则抛出异常
     * @param ep 端点信息，包括host+port
     * @param shard 分片
     * @param lock 分片锁
     * @param timeout_ms 超时时间，单位毫秒，0表示不超时
     * @return 连接文件描述符
     */
    int CreateNewConnection(const Endpoint &ep, PoolShard &shard,
                            std::unique_lock<std::mutex> &lock,
                            time_t timeout_ms);

    /**
     * @brief 创建连接，如果超时则抛出异常
     * @param ep 端点信息，包括host+port
     * @param timeout_ms 超时时间，单位毫秒，0表示不超时
     * @return 连接文件描述符
     */
    int ConnectWithTimeout(const Endpoint &ep, time_t timeout_ms);

    // 清理线程
    void CleanerTask();

    static time_t Now() noexcept;

    // 构建地址结构体
    static sockaddr_in BuildAddress(const Endpoint &ep);

    // 获取socket错误
    static int GetSocketError(int fd);

    // 关闭文件描述符
    static void CloseFd(int fd) noexcept;

    // 最大连接数
    const size_t max_conn_;
    // 每个分片最大空闲连接数
    const size_t max_idle_per_shard_ = 64;
    // 连接超时时间
    const time_t idle_timeout_; // 秒
    // 清理线程间隔
    const time_t cleaner_interval_; // 秒

    std::vector<PoolShard> shards_;
    std::hash<Endpoint> hash_fn_;
    std::thread cleaner_;
    GlobalState state_;
};

#endif