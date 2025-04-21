/**
 * @brief Redis客户端封装类, 包括连接redis服务器，发布消息，订阅消息，以及接收订阅通道中的消息
 * @author EkerSun
 * @date 2025-4-15
 */

#ifndef REDIS_H
#define REDIS_H

#include <hiredis/hiredis.h>
#include <thread>
#include <functional>

class Redis
{
public:
    Redis();
    ~Redis();

    static Redis& Instance();
    // 是否已经连;
    bool is_connected();

    // 连接redis服务器
    bool connect();

    // 向redis指定的通道channel发布消息
    bool publish(std::string channel, std::string message);

    // 向redis指定的通道subscribe订阅消息
    bool subscribe(std::string channel);

    // 向redis指定的通道unsubscribe取消订阅消息
    bool unsubscribe(std::string channel);

    // 在独立线程中接收订阅通道中的消息
    void observer_channel_message();

    // 初始化向业务层上报通道消息的回调对象
    void init_notify_handler(std::function<void(std::string, std::string)> fn);

    // 添加用户连接信息
    bool hset(int user_id, const std::string &conn_info);

    // 删除用户的连接信息
    bool hdel(int user_id);

    // 获取用户连接信息
    bool hget(int user_id, std::string &conn_info);

private:
    // hiredis同步上下文对象，负责publish消息
    redisContext *_publish_context;

    // hiredis同步上下文对象，负责subscribe消息
    redisContext *_subcribe_context;

    // 新增数据操作连接
    redisContext *_data_context;

    // 回调操作，收到订阅的消息，给service层上报
    std::function<void(std::string, std::string)> _notify_message_handler;
};

#endif
