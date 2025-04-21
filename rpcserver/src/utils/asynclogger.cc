#include "asynclogger.h"

AsyncLogger &AsyncLogger::GetInstance()
{
    static AsyncLogger instance;
    return instance;
}

LogLevel AsyncLogger::GetLogLevel() const { return log_level_; }
void AsyncLogger::SetLogLevel(LogLevel level) { log_level_.store(level); }
void AsyncLogger::SetFlushInterval(int flush_time) { flush_interval_ = flush_time; }
void AsyncLogger::SetFlushThreshold(size_t threshold) { flush_threshold_ = threshold; }
void AsyncLogger::SetOverflowPolicy(OverflowPolicy policy) { queue_.SetOverflowPolicy(policy); }
void AsyncLogger::SetOverflowCallback(OverflowCallback callback) { queue_.SetOverflowCallback(callback); }

AsyncLogger::AsyncLogger()
{
    RollLogFile();
    worker_ = std::thread(&AsyncLogger::ConsumerThread, this);
}

AsyncLogger::~AsyncLogger()
{
    running_.store(false);
    if (worker_.joinable())
        worker_.join();
    FlushRemaining();
}

void AsyncLogger::FlushRemaining()
{
    std::lock_guard<std::mutex> lock(log_file_.fp_mutex);
    auto remaining = queue_.PopAll();
    if (!remaining.empty())
    {
        WriteToFile(remaining);
    }
}

void AsyncLogger::RollLogFile()
{
    std::lock_guard<std::mutex> lock(log_file_.fp_mutex);

    if (log_file_.fp)
        fclose(log_file_.fp);

    time_t now = time(nullptr);
    struct tm tm_buf;
    localtime_r(&now, &tm_buf);

    char new_date[20];
    strftime(new_date, sizeof(new_date), "%Y-%m-%d", &tm_buf);

    char filename[128];
    snprintf(filename, sizeof(filename), "%s-log.txt", new_date);

    log_file_.fp = fopen(filename, "a+");
    if (!log_file_.fp)
    {
        // 降级处理：输出到标准错误
        fprintf(stderr, "[FATAL] Failed to open logfile: %s\n", filename);
        log_file_.fp = stderr;
    }
    log_file_.date_str = new_date;
}

void AsyncLogger::ConsumerThread()
{
    time_t next_roll = CalculateNextRollTime();
    while (running_.load(std::memory_order_relaxed))
    {
        // 批量处理日志
        auto batch = queue_.PopAll();
        if (!batch.empty() && batch.size() >= flush_threshold_)
        {
            WriteToFile(batch);
        }
        // 文件滚动检查
        if (time(nullptr) >= next_roll)
        {
            std::lock_guard<std::mutex> lock(log_file_.fp_mutex);
            RollLogFile();
            next_roll = CalculateNextRollTime();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(flush_interval_));
    }
}
void AsyncLogger::WriteToFile(const std::vector<std::string> &batch)
{
    std::lock_guard<std::mutex> lock(log_file_.fp_mutex);

    for (const auto &msg : batch)
    {
        fwrite(msg.data(), 1, msg.size(), log_file_.fp);
        fputc('\n', log_file_.fp);
    }
    fflush(log_file_.fp); // 保证立即写入磁盘
}
time_t AsyncLogger::CalculateNextRollTime()
{
    time_t now = time(nullptr);
    struct tm tm_buf;
    localtime_r(&now, &tm_buf);
    tm_buf.tm_hour = 0;
    tm_buf.tm_min = 0;
    tm_buf.tm_sec = 0;
    tm_buf.tm_mday += 1; // 次日凌晨
    return mktime(&tm_buf);
}

const char *AsyncLogger::LevelToString(LogLevel level)
{
    switch (level)
    {
    case LogLevel::TRACE:
        return "TRACE";
    case LogLevel::DEBUG:
        return "DEBUG";
    case LogLevel::INFO:
        return "INFO";
    case LogLevel::WARN:
        return "WARN";
    case LogLevel::ERROR:
        return "ERROR";
    case LogLevel::FATAL:
        return "FATAL";
    default:
        return "UNKNOWN";
    }
}