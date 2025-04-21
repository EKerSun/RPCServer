/**
 * @author EkerSun
 * @date 2025.3.28
 * @brief 自定义的控制器类，目前未设计具体方法
 */

#ifndef RPCCONTROLLER_H
#define RPCCONTROLLER_H

#include <google/protobuf/service.h>
#include <string>
#include "muduo/net/TcpConnection.h"
#include <mutex>

class TheRpcController : public google::protobuf::RpcController, public std::enable_shared_from_this<TheRpcController>
{
public:
    TheRpcController();
    void Reset();
    bool Failed() const;
    std::string ErrorText() const;
    void SetFailed(const std::string &reason);
    void SetConnection(const muduo::net::TcpConnectionPtr &conn);
    muduo::net::TcpConnectionPtr GetConnection() const;

    void StartCancel();
    bool IsCanceled() const;
    void NotifyOnCancel(google::protobuf::Closure *callback);

private:
    bool failed_;          // RPC方法执行过程中的状态
    bool canceled_;        // RPC方法执行过程中是否被取消
    std::string err_text_; // RPC方法执行过程中的错误信息
    mutable std::mutex mutex_;
    google::protobuf::Closure *cancel_callback_;
    std::weak_ptr<muduo::net::TcpConnection> connection_;
};

#endif