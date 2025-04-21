
#ifndef ASYNCLOGGER_H
#define ASYNCLOGGER_H

#include "doublebufferqueue.h"
#include <atomic>
#include <chrono>
#include <thread>
#include <string>
#include <mutex>
#include <cstdio>
#include <format>
#include <vector>

// #define LOG(LEVEL, FMT, ...) \
//     AsyncLogger::GetInstance().Log(LogLevel::LEVEL, __FILE__, __LINE__, __FUNCTION__, FMT, ##__VA_ARGS__)
// #define LOG_TRACE(...) LOG(TRACE, __VA_ARGS__)
// #define LOG_DEBUG(...) LOG(DEBUG, __VA_ARGS__)
// #define LOG_INFO(...) LOG(INFO, __VA_ARGS__)
// #define LOG_WARN(...) LOG(WARN, __VA_ARGS__)
// #define LOG_ERROR(...) LOG(ERROR, __VA_ARGS__)
// #define LOG_FATAL(...) LOG(FATAL, __VA_ARGS__)

#define LOG_STREAM(LEVEL) \
    AsyncLogger::LogStream(LogLevel::LEVEL, __FILE__, __LINE__, __FUNCTION__)
#define LOG_TRACE LOG_STREAM(TRACE)
#define LOG_DEBUG LOG_STREAM(DEBUG)
#define LOG_INFO LOG_STREAM(INFO)
#define LOG_WARN LOG_STREAM(WARN)
#define LOG_ERROR LOG_STREAM(ERROR)
#define LOG_FATAL LOG_STREAM(FATAL)

// 定义日志级别
enum class LogLevel
{
    TRACE = 0, // 跟踪信息
    DEBUG = 1, // 调试信息
    INFO = 2,  // 常规信息
    WARN = 3,  // 警告信息
    ERROR = 4, // 错误信息
    FATAL = 5  // 致命错误
};

// 异步日志器
class AsyncLogger
{
public:
    static AsyncLogger &GetInstance();
    LogLevel GetLogLevel() const;
    void SetLogLevel(LogLevel level);
    void SetFlushInterval(int flush_time);
    void SetFlushThreshold(size_t threshold);
    void SetOverflowPolicy(OverflowPolicy policy);
    void SetOverflowCallback(OverflowCallback callback);

    // 写入日志队列, 包括, 日志级别、文件名、行号、函数名、格式化参数
    template <typename... Args>
    void Log(LogLevel level, const char *file, int line, const char *func, const std::string &fmt, Args &&...args)
    {
        if (level < log_level_.load(std::memory_order_relaxed))
            return;
        std::string message = FormatMessage(level, file, line, func, fmt, std::forward<Args>(args)...);
        queue_.Push(std::move(message));
    }

    void Log(LogLevel level, const char *file, int line, const char *func, const std::string &message)
    {
        if (level < log_level_.load(std::memory_order_relaxed))
            return;
        queue_.Push(FormatMessage(level, file, line, func, message));
    }
    class LogStream
    {
    public:
        LogStream(LogLevel level, const char *file, int line, const char *func)
            : level_(level), file_(file), line_(line), func_(func) {}

        ~LogStream()
        {
            if (level_ >= AsyncLogger::GetInstance().GetLogLevel())
            {
                AsyncLogger::GetInstance().Log(
                    level_, file_, line_, func_, ss_.str());
            }
        }

        template <typename T>
        LogStream &operator<<(T &&val)
        {
            ss_ << std::forward<T>(val);
            return *this;
        }

    private:
        std::ostringstream ss_;
        LogLevel level_;
        const char *file_;
        int line_;
        const char *func_;
    };

private:
    // 文件控制块
    struct LogFile
    {
        FILE *fp = nullptr;
        std::string date_str;
        std::mutex fp_mutex;
    };
    // 日志级别
    std::atomic<LogLevel> log_level_{LogLevel::INFO};
    // 日志队列
    DoubleBufferQueue<std::string> queue_;
    // 运行状态
    std::atomic<bool> running_{true};
    // 工作线程
    std::thread worker_;
    // 日志文件
    LogFile log_file_;
    // 刷盘间隔, 单位ms
    int flush_interval_{10};
    // 日志阈值
    size_t flush_threshold_ = 0;
    // 构造函数
    AsyncLogger();

    ~AsyncLogger();

    void FlushRemaining();
    void RollLogFile();
    void ConsumerThread();
    void WriteToFile(const std::vector<std::string> &batch);
    time_t CalculateNextRollTime();

    // 修改FormatMessage函数，增加微秒精度
    template <typename... Args>
    std::string FormatMessage(LogLevel level, const char *file, int line,
                              const char *func, const std::string &fmt, Args &&...args)
    {
        auto now = std::chrono::system_clock::now();
        auto now_time = std::chrono::system_clock::to_time_t(now);
        auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
            now.time_since_epoch() % std::chrono::seconds(1));

        struct tm tm_buf;
        localtime_r(&now_time, &tm_buf);

        char time_str[32];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm_buf);
        snprintf(time_str + 19, sizeof(time_str) - 19, ".%06ld", now_us.count());
        std::string formatted_msg = std::vformat(fmt, std::make_format_args(args...));

        return std::format("[{}] [{}:{}@{}] {} - {}",
                           time_str,
                           LevelToString(level),
                           file, line, func,
                           formatted_msg);
    }
    const char *LevelToString(LogLevel level);
};

#endif
