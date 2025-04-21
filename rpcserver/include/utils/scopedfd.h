#ifndef SCOPEDFD_H
#define SCOPEDFD_H

// 封装文件描述符，支持移动语义和显式转换
class ScopedFd
{
public:
    // 构造函数
    explicit ScopedFd(int fd = -1);

    // 支持移动语义
    ScopedFd(ScopedFd &&other) noexcept;

    // 析构函数
    ~ScopedFd();

    // 获取文件描述符
    int get() const noexcept;

    // 释放文件描述符
    int release() noexcept;

    // 重置文件描述符
    void Reset(int fd = -1) noexcept;

    // 显式转换成bool
    explicit operator bool() const noexcept;

    // 隐式转换成int
    operator int() const;

private:
    // 文件描述符
    int fd_;
    // 禁止复制
    ScopedFd(const ScopedFd &) = delete;
    // 禁止赋值
    ScopedFd &operator=(const ScopedFd &) = delete;
};

#endif