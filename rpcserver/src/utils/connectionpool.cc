#include "connectionpool.h"
#include "poolexecption.h"

/**
 * @brief 构造函数，实现初始化epoll和启动清理线程
 * @param max_conn 最大连接数
 * @param shard_num 分片数
 * @param idle_timeout 空闲超时时间
 */
ConnectionPool::ConnectionPool(size_t max_conn,
                               size_t shard_num,
                               time_t idle_timeout)
    : max_conn_(max_conn),
      shards_(shard_num),
      idle_timeout_(idle_timeout),
      cleaner_interval_(idle_timeout / 2)
{

    // 初始化全局epoll
    state_.epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (state_.epoll_fd < 0)
    {
        throw ConnectionError(errno, "epoll_create1 failed");
    }
    // 启动清理线程
    cleaner_ = std::thread([this]
                           { CleanerTask(); });
}
/**
 * @brief 析构函数，实现停止清理线程和关闭全局epoll
 */
ConnectionPool::~ConnectionPool()
{
    // 设置全局运行状态为false
    state_.running.store(false, std::memory_order_release);
    // 等待清理线程结束
    if (cleaner_.joinable())
        cleaner_.join();
    // 关闭全局epoll
    close(state_.epoll_fd);
}

// 获取单例
ConnectionPool &ConnectionPool::GetInstance()
{
    static ConnectionPool pool;
    return pool;
}

/**
 * @brief 获取一个连接，如果超时则抛出异常
 * @param ep 端点信息，包括host+port
 * @param timeout_ms 超时时间，单位毫秒，0表示不超时
 * @return 连接文件描述符
 */
int ConnectionPool::Get(const Endpoint &ep, time_t timeout_ms)
{
    // 获取分片
    auto &shard = GetShard(ep);
    std::unique_lock lock(shard.mutex);

    // 快速路径：尝试获取空闲连接
    while (!shard.idle_fds.empty())
    {
        // 获取一个空闲连接
        int fd = shard.idle_fds.front();
        // 移除空闲连接
        shard.idle_fds.pop();

        if (Validate(fd))
        {
            shard.active_count++;
            return fd;
        }
        // 关闭无效连接
        CloseFd(fd);
    }

    // 慢速路径：创建新连接
    return CreateNewConnection(ep, shard, lock, timeout_ms);
}

/**
 * @brief 释放一个连接，如果连接无效则关闭
 * @param fd 连接文件描述符
 * @param ep 端点信息，包括host+port
 */
void ConnectionPool::Release(int fd, const Endpoint &ep) noexcept
{
    auto &shard = GetShard(ep);
    std::lock_guard lock(shard.mutex);

    // 如果空闲连接数小于最大空闲连接数，则放入空闲队列中，否则关闭连接
    if (shard.idle_fds.size() < max_idle_per_shard_)
    {
        shard.idle_fds.push(fd);
        shard.active_count--;
        shard.last_active = Now();
    }
    else
    {
        CloseFd(fd);
    }

    // 唤醒等待的线程
    if (state_.waiters > 0)
    {
        shard.cv.notify_one();
    }
}

ConnectionPool::PoolShard &ConnectionPool::GetShard(const Endpoint &ep)
{
    size_t idx = hash_fn_(ep) % shards_.size();
    return shards_[idx];
}

// 检查连接是否有效
bool ConnectionPool::Validate(int fd) noexcept
{
    // 通过getsockopt检查socket状态
    int error = 0;
    socklen_t len = sizeof(error);
    return getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) == 0 &&
           error == 0;
}

/**
 * @brief 创建新连接，如果超时则抛出异常
 * @param ep 端点信息，包括host+port
 * @param shard 分片
 * @param lock 分片锁
 * @param timeout_ms 超时时间，单位毫秒，0表示不超时
 * @return 连接文件描述符
 */
int ConnectionPool::CreateNewConnection(const Endpoint &ep, PoolShard &shard,
                                        std::unique_lock<std::mutex> &lock,
                                        time_t timeout_ms)
{
    time_t start = Now();

    while (true)
    {
        // 如果连接数超过最大连接数，则等待连接释放
        if (state_.total_conn >= max_conn_)
        {
            // 如果超时，则抛出异常
            if (timeout_ms == 0)
            {
                throw PoolExhausted("Connection pool exhausted");
            }

            // 等待连接释放
            state_.waiters++;
            auto cv_status = shard.cv.wait_for(
                lock,
                std::chrono::milliseconds(timeout_ms),
                [this]
                { return state_.total_conn < max_conn_; });
            state_.waiters--;

            // 超时，则抛出异常
            if (!cv_status)
            {
                throw PoolTimeout("Wait for connection timeout");
            }
            // 计算剩余超时时间
            timeout_ms -= (Now() - start);
            continue;
        }

        // 实际创建连接
        int fd = ConnectWithTimeout(ep, timeout_ms);
        state_.total_conn.fetch_add(1, std::memory_order_relaxed);
        shard.active_count++;
        return fd;
    }
}

/**
 * @brief 创建连接，如果超时则抛出异常
 * @param ep 端点信息，包括host+port
 * @param timeout_ms 超时时间，单位毫秒，0表示不超时
 * @return 连接文件描述符
 */
int ConnectionPool::ConnectWithTimeout(const Endpoint &ep, time_t timeout_ms)
{
    // 创建socket
    ScopedFd fd(socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0));
    if (fd < 0)
    {
        throw ConnectionError(errno, "socket() failed");
    }

    // 根据ep构建地址结构体
    sockaddr_in addr = BuildAddress(ep);

    // 异步连接
    int ret = connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));

    // 立即连接成功
    if (ret == 0)
    {

        return fd.release();
    }
    // 连接失败，则抛出异常
    if (errno != EINPROGRESS)
    {
        throw ConnectionError(errno, "connect() failed");
    }

    // 没有连接成功，则等待连接完成

    epoll_event event{};               // 注册到epoll
    event.events = EPOLLOUT | EPOLLET; // 边缘触发
    event.data.fd = fd;
    // 注册到epoll
    if (epoll_ctl(state_.epoll_fd, EPOLL_CTL_ADD, fd, &event) < 0)
    {
        throw ConnectionError(errno, "epoll_ctl failed");
    }

    // 等待事件
    epoll_event events[1];
    int num = epoll_wait(state_.epoll_fd, events, 1, timeout_ms);
    if (num <= 0)
    {
        if (num == 0)
        {
            throw PoolTimeout("Connect timeout");
        }
        else
        {
            throw ConnectionError(errno, "epoll_wait failed");
        }
    }

    // 检查连接结果
    int err = GetSocketError(fd);
    if (err != 0)
    {
        throw ConnectionError(err, "connect failed");
    }

    return fd.release();
}

// 清理线程
void ConnectionPool::CleanerTask()
{
    // 当运行标志为true时，每隔cleaner_interval_秒清理一次空闲连接
    while (state_.running.load(std::memory_order_acquire))
    {
        std::this_thread::sleep_for(std::chrono::seconds(cleaner_interval_));

        time_t now = Now();
        for (auto &shard : shards_)
        {
            std::lock_guard lock(shard.mutex);

            size_t cleaned = 0;
            // 最多清理一半
            const size_t max_clean = shard.idle_fds.size() / 2;

            while (!shard.idle_fds.empty() && cleaned < max_clean)
            {
                int fd = shard.idle_fds.front();
                // 如果连接超时，或者连接无效，则关闭连接并清理
                if (now - shard.last_active > idle_timeout_ || !Validate(fd))
                {
                    shard.idle_fds.pop();
                    CloseFd(fd);
                    cleaned++;
                }
                else
                {
                    break; // 后续连接可能未超时
                }
            }
        }
    }
}

time_t ConnectionPool::Now() noexcept
{
    using namespace std::chrono;
    return duration_cast<seconds>(
               steady_clock::now().time_since_epoch())
        .count();
}

// 构建地址结构体
sockaddr_in ConnectionPool::BuildAddress(const Endpoint &ep)
{
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(ep.port);
    if (inet_pton(AF_INET, ep.host.c_str(), &addr.sin_addr) <= 0)
    {
        throw std::invalid_argument("Invalid address: " + ep.host);
    }
    return addr;
}

// 获取socket错误
int ConnectionPool::GetSocketError(int fd)
{
    int err = 0;
    socklen_t len = sizeof(err);
    return getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len) == 0 ? err
                                                                 : errno;
}

// 关闭文件描述符
void ConnectionPool::CloseFd(int fd) noexcept
{
    if (fd >= 0)
    {
        shutdown(fd, SHUT_RDWR);
        close(fd);
    }
}
