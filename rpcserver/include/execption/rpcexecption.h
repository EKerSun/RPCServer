#ifndef RPC_EXCEPTION_H
#define RPC_EXCEPTION_H

#include <string>
#include <exception>

enum class RpcErrorType
{
    SUCCESS,             // 成功（非错误）
    NETWORK_ERROR,       // 网络层错误（触发熔断）
    TIMEOUT,             // 请求超时（触发熔断）
    SERVICE_UNAVAILABLE, // 服务不可用（触发熔断）
    PROTOCOL_ERROR,      // 协议解析错误
    BUSINESS_ERROR,      // 业务逻辑错误（不触发熔断）
    UNAUTHORIZED,        // 鉴权失败
    RESOURCE_EXHAUSTED,  // 资源耗尽
    CONFIG_ERROR,        // 配置错误
    INVALID_RESPONSE,    // 无效响应
    SYSTEM_ERROR         // 系统错误（不触发熔断）
};
class RpcException : public std::exception
{
public:
    // 直接根据错误类型构造
    explicit RpcException(RpcErrorType type, std::string msg = "")
        : type_(type), msg_(std::move(msg))
    {
        if (msg_.empty())
        {
            msg_ = DefaultErrorMessage(type);
        }
    }

    // 左值构造
    explicit RpcException(const std::string &msg,
                          RpcErrorType type = RpcErrorType::BUSINESS_ERROR)
        : type_(type), msg_(msg) {}
    // 移动构造
    explicit RpcException(std::string &&msg,
                          RpcErrorType type = RpcErrorType::BUSINESS_ERROR)
        : type_(type), msg_(std::move(msg)) {}

    // 返回错误类型
    RpcErrorType type() const noexcept { return type_; }

    // 异常信息（noexcept保证不抛异常）
    const char *what() const noexcept override
    {
        return msg_.c_str();
    }

    // 快速创建网络层错误
    static RpcException NetworkError(const std::string &details)
    {
        return RpcException(RpcErrorType::NETWORK_ERROR,
                            "Network error: " + details);
    }

    // 快速创建超时错误
    static RpcException Timeout(const std::string &service)
    {
        return RpcException(RpcErrorType::TIMEOUT,
                            "Timeout reaching service: " + service);
    }

    // 快速创建服务不可用错误
    static RpcException ServiceUnavailable(const std::string &service)
    {
        return RpcException(RpcErrorType::SERVICE_UNAVAILABLE,
                            "Service unavailable: " + service);
    }

private:
    // 根据错误类型返回默认错误信息
    static std::string DefaultErrorMessage(RpcErrorType type)
    {
        switch (type)
        {
        case RpcErrorType::NETWORK_ERROR:
            return "Network communication failure";
        case RpcErrorType::TIMEOUT:
            return "Request timed out";
        case RpcErrorType::SERVICE_UNAVAILABLE:
            return "Service temporarily unavailable";
        case RpcErrorType::PROTOCOL_ERROR:
            return "Protocol parsing error";
        case RpcErrorType::UNAUTHORIZED:
            return "Authentication failed";
        case RpcErrorType::RESOURCE_EXHAUSTED:
            return "Resource limit exceeded";
        case RpcErrorType::CONFIG_ERROR:
            return "Invalid configuration";
        default:
            return "Unknown RPC error";
        }
    }

    RpcErrorType type_;
    std::string msg_; // 保证异常信息内存安全
};

#endif