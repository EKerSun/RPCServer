#ifndef PoolExecption_H
#define PoolExecption_H

#include <exception>
#include <stdexcept>
#include <system_error>

// 连接池超时异常
class PoolTimeout : public std::runtime_error
{
public:
    using runtime_error::runtime_error;
};

// 连接池满异常
class PoolExhausted : public std::runtime_error
{
public:
    using runtime_error::runtime_error;
};

// 连接异常
class ConnectionError : public std::system_error
{
public:
    using system_error::system_error;

    ConnectionError(int ev, const char *msg)
        : system_error(ev, std::generic_category(), msg) {}
};

#endif