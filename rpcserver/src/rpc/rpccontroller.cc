#include "rpccontroller.h"
#include "muduo/net/EventLoop.h"
#include <mutex>
TheRpcController::TheRpcController()
    : failed_(false), canceled_(false), cancel_callback_(nullptr) {}

void TheRpcController::Reset()
{
    failed_ = false;
    err_text_.clear();
    canceled_ = false;
    cancel_callback_ = nullptr;
    connection_.reset();
}

bool TheRpcController::Failed() const
{
    return failed_;
}
std::string TheRpcController::ErrorText() const
{
    return err_text_;
}

void TheRpcController::SetFailed(const std::string &reason)
{
    failed_ = true;
    err_text_ = reason;
}

void TheRpcController::SetConnection(const muduo::net::TcpConnectionPtr &conn)
{
    connection_ = conn;
}
muduo::net::TcpConnectionPtr TheRpcController::GetConnection() const
{
    return connection_.lock();
}

void TheRpcController::StartCancel()
{
    muduo::net::TcpConnectionPtr conn;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        canceled_ = true;
        conn = connection_.lock();
    }
    if (cancel_callback_)
    {
        auto callback = cancel_callback_;
        if (conn)
        {
            conn->getLoop()->queueInLoop([callback]
                                         { callback->Run(); });
        }
        else
        {
            callback->Run();
        }
    }
}
bool TheRpcController::IsCanceled() const
{
    return canceled_;
}
void TheRpcController::NotifyOnCancel(google::protobuf::Closure *callback)
{
    std::lock_guard<std::mutex> lock(mutex_);
    cancel_callback_ = callback;
}