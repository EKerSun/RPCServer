#include "circuitbreaker.h"
CircuitBreaker::CircuitBreaker(CircuitBreaker::Config config) : config_(config) {}

bool CircuitBreaker::AllowRequest()
{
    // 无锁快速路径检查
    if (state_.load(std::memory_order_acquire) == State::CLOSED)
        return true;

    // 加锁路径
    std::lock_guard<std::mutex> lock(mutex_);

    // 全开状态检查
    if (state_ == State::OPEN)
    {
        const auto now = Clock::now();
        // 超时重置熔断器
        if (now - last_failure_ >= config_.reset_timeout)
        {
            TransitionToState(State::HALF_OPEN);
            half_open_requests_ = 0;
            half_open_successes_ = 0;
            return true;
        }
        return false;
    }

    // 半开状态检查
    if (state_ == State::HALF_OPEN)
    {
        return half_open_requests_++ < config_.half_open_max_requests;
    }

    return false;
}

void CircuitBreaker::RecordSuccess()
{
    std::lock_guard<std::mutex> lock(mutex_);
    // 半开状态检查
    if (state_ == State::HALF_OPEN)
    {
        // 成功次数达到阈值，重置熔断器
        if (++half_open_successes_ >= config_.success_threshold)
        {
            TransitionToState(State::CLOSED);
            failures_.store(0, std::memory_order_release);
        }
    }
    else
    {
        // 重置失败计数
        failures_.store(0, std::memory_order_release);
    }
}

void CircuitBreaker::RecordFailure()
{
    std::lock_guard<std::mutex> lock(mutex_);

    // 失败次数累加，记录时间戳
    failures_.fetch_add(1, std::memory_order_acq_rel);
    last_failure_ = Clock::now();

    // 触发熔断器
    if (state_ == State::HALF_OPEN ||
        failures_.load(std::memory_order_acquire) >= config_.failure_threshold)
    {
        TransitionToState(State::OPEN);
    }
}

CircuitBreaker::State CircuitBreaker::GetState() const
{
    return state_.load(std::memory_order_acquire);
}

void CircuitBreaker::TransitionToState(CircuitBreaker::State new_state)
{
    state_.store(new_state, std::memory_order_release);
}

CircuitBreaker &CircuitBreakerManager::GetInstance(const std::string &service_name)
{
    static std::mutex instance_mutex;
    std::lock_guard<std::mutex> lock(instance_mutex);

    // 在第一次访问时创建熔断器
    auto &breaker = breakers_[service_name];
    // 如果熔断器不存在，则创建一个新实例
    if (!breaker)
    {
        breaker = std::make_unique<CircuitBreaker>(GetConfigForService(service_name));
    }
    return *breaker;
}

CircuitBreaker::Config CircuitBreakerManager::GetConfigForService(const std::string &service)
{
    // 目前设置返回默认熔断器
    CircuitBreaker::Config default_config;
    return default_config;
}