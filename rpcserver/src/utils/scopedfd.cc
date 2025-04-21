#include "scopedfd.h"
#include <unistd.h>

// 构造函数
ScopedFd::ScopedFd(int fd) : fd_(fd) {}

// 支持移动语义
ScopedFd::ScopedFd(ScopedFd &&other) noexcept : fd_(other.fd_)
{
    other.fd_ = -1;
}

// 析构函数
ScopedFd::~ScopedFd()
{
    Reset();
}

// 获取文件描述符
int ScopedFd::get() const noexcept { return fd_; }

// 释放文件描述符
int ScopedFd::release() noexcept
{
    int ret = fd_;
    fd_ = -1;
    return ret;
}

// 重置文件描述符
void ScopedFd::Reset(int fd) noexcept
{
    if (fd_ >= 0)
        close(fd_);
    fd_ = fd;
}

// 显式转换成bool
ScopedFd::operator bool() const noexcept
{
    return fd_ >= 0;
}

// 隐式转换成int
ScopedFd::operator int() const
{
    return fd_;
}
