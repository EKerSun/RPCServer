#ifndef RPCCLOSURE_H
#define RPCCLOSURE_H
#include "muduo/net/EventLoop.h"
#include <google/protobuf/service.h>
class RpcClosure : public google::protobuf::Closure
{
public:
    using Callback = std::function<void()>;
    RpcClosure(muduo::net::EventLoop *loop, Callback &&cb)
        : loop_(loop),
          callback_(std::move(cb)),
          destroyed_(std::make_shared<bool>(false)) {}

    ~RpcClosure() override
    {
        *destroyed_ = true;
    }

    void Run() override
    {
        // 使用weak_ptr检测对象是否已被销毁
        auto destroyed = destroyed_;
        if (loop_->isInLoopThread())
        {
            if (!*destroyed)
                callback_();
        }
        else
        {
            loop_->queueInLoop([this, destroyed]
                               {
                    if (!*destroyed) callback_(); });
        }
        delete this;
    }

private:
    muduo::net::EventLoop *loop_;
    Callback callback_;
    std::shared_ptr<bool> destroyed_; // 用于检测回调是否有效
};

#endif