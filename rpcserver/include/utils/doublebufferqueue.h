#ifndef DOUBLEBUFFERQUEUE_HPP
#define DOUBLEBUFFERQUEUE_HPP

#include <vector>
#include <atomic>
#include <chrono>
#include <thread>
#include <functional>

// 溢出策略
enum class OverflowPolicy
{
    DiscardNewest,
    DiscardOldest,
    BlockWithTimeout
};

// 处理溢出的回调函数，供外部使用
using OverflowCallback = std::function<void(size_t lost_count)>;

template <typename T>
class DoubleBufferQueue
{
public:
    // 构造函数
    explicit DoubleBufferQueue(size_t buffer_size = 10000);

    // 生产者API：非阻塞追加数据
    bool Push(const T &item) noexcept;

    // 消费者API：交换缓冲区并获取全部数据
    std::vector<T> PopAll() noexcept;

    // 设置溢出回调函数
    void SetOverflowCallback(OverflowCallback cb);

    // 设置溢出策略
    void SetOverflowPolicy(OverflowPolicy policy);

private:
    struct Buffer
    {
        std::vector<T> data;
        std::atomic<size_t> size{0};
    };

    // 双缓冲区
    Buffer buffers_[2];
    // 当前写入缓冲区索引
    std::atomic<int> write_index_{0};
    // 缓冲区容量
    const size_t buffer_size_;
    // 溢出回调
    OverflowCallback overflow_callback_;
    // 丢失计数器
    std::atomic<size_t> lost_counter_{0};
    // 溢出策略
    OverflowPolicy overflow_policy_{OverflowPolicy::DiscardNewest};

    // 获取当前写缓冲区
    Buffer &GetWriteBuffer();

    // 获取当前读缓冲区
    Buffer &GetReadBuffer();

    // 交换缓冲区
    void SwapBuffers();

    // 处理溢出
    bool HandleOverflow(const T &item);
};

// 构造函数
template <typename T>
DoubleBufferQueue<T>::DoubleBufferQueue(size_t buffer_size)
    : buffer_size_(buffer_size)
{
    buffers_[0].data.resize(buffer_size_);
    buffers_[1].data.resize(buffer_size_);
}

// 生产者API：非阻塞追加数据
template <typename T>
bool DoubleBufferQueue<T>::Push(const T &item) noexcept
{
    // 获取当前写入缓冲区
    Buffer &write_buf = GetWriteBuffer();

    // 缓冲区已满，执行溢出处理
    if (write_buf.size.load(std::memory_order_relaxed) >= buffer_size_)
    {
        return HandleOverflow(item);
    }

    // 获取当前写入位置
    size_t pos = write_buf.size.fetch_add(1, std::memory_order_relaxed);
    if (pos >= buffer_size_)
    {
        write_buf.size.store(buffer_size_, std::memory_order_relaxed);
        return HandleOverflow(item);
    }
    // 移动构造，避免拷贝开销
    write_buf.data[pos] = std::move(item);
    return true;
}

// 消费者API：交换缓冲区并获取全部数据
template <typename T>
std::vector<T> DoubleBufferQueue<T>::PopAll() noexcept
{
    // 获取当前写入缓冲区
    Buffer &write_buf = GetWriteBuffer();
    if (write_buf.size.load(std::memory_order_acquire) == 0)
    {
        return {}; // 如果写缓冲区为空，直接返回
    }

    // 交换缓冲区并获取当前读缓冲区
    SwapBuffers();
    Buffer &read_buf = GetReadBuffer();

    // 读取数据并重置读缓冲区
    std::vector<T> result;
    const size_t valid_size = read_buf.size.load(std::memory_order_acquire);
    result.reserve(valid_size);
    for (size_t i = 0; i < valid_size; ++i)
    {
        result.emplace_back(std::move(read_buf.data[i]));
    }
    read_buf.size.store(0, std::memory_order_release);
    return result;
}

template <typename T>
void DoubleBufferQueue<T>::SetOverflowCallback(OverflowCallback cb)
{
    overflow_callback_ = std::move(cb);
}

template <typename T>
void DoubleBufferQueue<T>::SetOverflowPolicy(OverflowPolicy policy)
{
    overflow_policy_ = policy;
}

// 获取当前写缓冲区
template <typename T>
typename DoubleBufferQueue<T>::Buffer &DoubleBufferQueue<T>::GetWriteBuffer()
{
    return buffers_[write_index_.load(std::memory_order_relaxed)];
}

// 获取当前读缓冲区
template <typename T>
typename DoubleBufferQueue<T>::Buffer &DoubleBufferQueue<T>::GetReadBuffer()
{
    return buffers_[1 - write_index_.load(std::memory_order_relaxed)];
}

// 交换缓冲区
template <typename T>
void DoubleBufferQueue<T>::SwapBuffers()
{
    write_index_.store(1 - write_index_.load(std::memory_order_relaxed), std::memory_order_release);
}

// 处理溢出
template <typename T>
bool DoubleBufferQueue<T>::HandleOverflow(const T &item)
{
    switch (overflow_policy_)
    {
    case OverflowPolicy::DiscardNewest:
        // 丢弃最新数据
        if (overflow_callback_)
        {
            overflow_callback_(1);
        }
        return false;

    case OverflowPolicy::DiscardOldest:
    {
        // 丢弃最旧的数据
        Buffer &write_buf = GetWriteBuffer();
        lost_counter_.fetch_add(1, std::memory_order_relaxed);
        if (!write_buf.data.empty())
        {
            write_buf.data[0] = std::move(item); // 覆盖最旧数据
            if (overflow_callback_)
            {
                overflow_callback_(1);
            }
            return true;
        }
        return false;
    }

    case OverflowPolicy::BlockWithTimeout:
    {
        // 阻塞直至有空间，带超时
        const auto start = std::chrono::steady_clock::now();
        while (true)
        {
            Buffer &write_buf = GetWriteBuffer();
            if (write_buf.size.load(std::memory_order_relaxed) < buffer_size_)
            {
                return Push(std::move(item)); // 重试
            }

            if (std::chrono::steady_clock::now() - start >
                std::chrono::milliseconds(100)) // 超过100ms退出
            {
                if (overflow_callback_)
                {
                    overflow_callback_(1);
                }
                return false;
            }
            std::this_thread::yield();
        }
    }
    }
    return false;
}

#endif
