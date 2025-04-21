#ifndef CIRCUITBREAKER_H
#define CIRCUITBREAKER_H

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>

class CircuitBreaker
{
public:
    enum class State
    {
        CLOSED,   // 关闭状态，允许请求
        OPEN,     // 开启状态，不允许请求
        HALF_OPEN // 半开状态，允许请求，但最多允许一定数量的请求
    };

    struct Config
    {
        uint32_t failure_threshold = 3;        // 触发熔断的连续失败次数
        std::chrono::seconds reset_timeout{5}; // 熔断后进入半开状态的时间
        uint32_t half_open_max_requests = 5;   // 半开状态最大允许请求数
        uint32_t success_threshold = 3;        // 半开状态下成功次数阈值
        explicit Config(uint32_t failure = 3,
                        std::chrono::seconds timeout = std::chrono::seconds(5),
                        uint32_t half_open_max = 5,
                        uint32_t success = 3)
            : failure_threshold(failure),
              reset_timeout(timeout),
              half_open_max_requests(half_open_max),
              success_threshold(success) {}
    };

    explicit CircuitBreaker(Config config = Config());

    bool AllowRequest();

    void RecordSuccess();

    void RecordFailure();

    State GetState() const;

private:
    using Clock = std::chrono::steady_clock;

    void TransitionToState(State new_state);

    mutable std::mutex mutex_;
    Config config_;

    // 原子状态保证无锁读取
    std::atomic<State> state_{State::CLOSED};

    // 失败计数需要原子操作
    std::atomic<uint32_t> failures_{0};

    // 保护时间戳和半开状态计数器
    Clock::time_point last_failure_;

    // 半开状态计数器
    uint32_t half_open_requests_ = 0;
    uint32_t half_open_successes_ = 0;
};

// 熔断器管理器（按服务名称管理）
class CircuitBreakerManager
{
public:
    static CircuitBreaker &GetInstance(const std::string &service_name);

private:
    static CircuitBreaker::Config GetConfigForService(const std::string &service);

    static inline std::unordered_map<std::string, std::unique_ptr<CircuitBreaker>> breakers_;
};

#endif // CIRCUITBREAKER_H