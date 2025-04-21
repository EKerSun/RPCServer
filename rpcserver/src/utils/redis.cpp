#include "redis.hpp"
#include <iostream>

Redis::Redis()
    : _publish_context(nullptr), _subcribe_context(nullptr), _data_context(nullptr) {}

Redis::~Redis()
{
    if (_publish_context != nullptr)
    {
        redisFree(_publish_context);
    }

    if (_subcribe_context != nullptr)
    {
        redisFree(_subcribe_context);
    }
    if (_data_context != nullptr)
    {
        redisFree(_data_context);
    }
}

Redis &Redis::Instance()
{
    static Redis instance;
    return instance;
}

bool Redis::is_connected()
{
    return (_publish_context != nullptr) &&
           (_subcribe_context != nullptr) &&
           (_data_context != nullptr);
}

bool Redis::connect()
{
    // 负责publish发布消息的上下文连接
    _publish_context = redisConnect("127.0.0.1", 6379);
    if (nullptr == _publish_context)
    {
        std::cerr << "connect redis failed!" << std::endl;
        return false;
    }

    // 负责subscribe订阅消息的上下文连接
    _subcribe_context = redisConnect("127.0.0.1", 6379);
    if (nullptr == _subcribe_context)
    {
        std::cerr << "connect redis failed!" << std::endl;
        return false;
    }

    // 新增数据操作连接
    _data_context = redisConnect("127.0.0.1", 6379);
    if (nullptr == _data_context)
    {
        std::cerr << "connect redis data context failed!" << std::endl;
        return false;
    }

    // 在单独的线程中，监听通道上的事件，有消息给业务层进行上报
    std::thread t([&]()
                  { observer_channel_message(); });
    t.detach();

    std::cout << "connect redis-server success!" << std::endl;

    return true;
}

// 向redis指定的通道channel发布消息
bool Redis::publish(std::string channel, std::string message)
{
    redisReply *reply = (redisReply *)redisCommand(_publish_context, "PUBLISH %s %s", channel.c_str(), message.c_str());
    if (nullptr == reply)
    {
        std::cerr << "publish command failed!" << std::endl;
        return false;
    }
    freeReplyObject(reply);
    return true;
}

// 向redis指定的通道subscribe订阅消息
bool Redis::subscribe(std::string channel)
{
    // SUBSCRIBE命令本身会造成线程阻塞等待通道里面发生消息，这里只做订阅通道，不接收通道消息
    // 通道消息的接收专门在observer_channel_message函数中的独立线程中进行
    // 只负责发送命令，不阻塞接收redis server响应消息，否则和notifyMsg线程抢占响应资源
    if (REDIS_ERR == redisAppendCommand(this->_subcribe_context, "SUBSCRIBE %s", channel.c_str()))
    {
        std::cerr << "subscribe command failed!" << std::endl;
        return false;
    }
    // redisBufferWrite可以循环发送缓冲区，直到缓冲区数据发送完毕（done被置为1）
    int done = 0;
    while (!done)
    {
        if (REDIS_ERR == redisBufferWrite(this->_subcribe_context, &done))
        {
            std::cerr << "subscribe command failed!" << std::endl;
            return false;
        }
    }
    // redisGetReply

    return true;
}

// 向redis指定的通道unsubscribe取消订阅消息
bool Redis::unsubscribe(std::string channel)
{
    if (REDIS_ERR == redisAppendCommand(this->_subcribe_context, "UNSUBSCRIBE %d", channel))
    {
        std::cerr << "unsubscribe command failed!" << std::endl;
        return false;
    }
    // redisBufferWrite可以循环发送缓冲区，直到缓冲区数据发送完毕（done被置为1）
    int done = 0;
    while (!done)
    {
        if (REDIS_ERR == redisBufferWrite(this->_subcribe_context, &done))
        {
            std::cerr << "unsubscribe command failed!" << std::endl;
            return false;
        }
    }
    return true;
}

// 在独立线程中接收订阅通道中的消息
void Redis::observer_channel_message()
{
    redisReply *reply = nullptr;
    while (REDIS_OK == redisGetReply(this->_subcribe_context, (void **)&reply))
    {
        // 订阅收到的消息是一个带三元素的数组
        if (reply != nullptr && reply->element[2] != nullptr && reply->element[2]->str != nullptr)
        {
            // 给业务层上报通道上发生的消息
            _notify_message_handler(reply->element[1]->str, reply->element[2]->str);
        }

        freeReplyObject(reply);
    }

    std::cerr << ">>>>>>>>>>>>> observer_channel_message quit <<<<<<<<<<<<<" << std::endl;
}

void Redis::init_notify_handler(std::function<void(std::string, std::string)> fn)
{
    this->_notify_message_handler = fn;
}

// 添加用户连接信息
bool Redis::hset(int user_id, const std::string &connID)
{
    redisReply *reply = (redisReply *)redisCommand(
        _data_context,
        "HSET user_connections %d %s",
        user_id,
        connID.c_str());

    if (nullptr == reply)
    {
        std::cerr << "hset command failed!" << std::endl;
        return false;
    }

    // 处理错误响应
    if (reply->type == REDIS_REPLY_ERROR)
    {
        std::cerr << "Redis error: " << reply->str << std::endl;
        freeReplyObject(reply);
        return false;
    }

    freeReplyObject(reply);
    return true;
}

// 删除用户的连接信息
bool Redis::hdel(int user_id)
{
    redisReply *reply = (redisReply *)redisCommand(
        _data_context,
        "HDEL user_connections %d",
        user_id);

    if (nullptr == reply)
    {
        std::cerr << "hdel command failed!" << std::endl;
        return false;
    }

    if (reply->type == REDIS_REPLY_ERROR)
    {
        std::cerr << "Redis error: " << reply->str << std::endl;
        freeReplyObject(reply);
        return false;
    }

    freeReplyObject(reply);
    return true;
}

// 根据用户id查询连接信息
bool Redis::hget(int user_id, std::string &connID)
{
    redisReply *reply = (redisReply *)redisCommand(
        _data_context,
        "HGET user_connections %d",
        user_id);

    if (nullptr == reply)
    {
        std::cerr << "hget command failed!" << std::endl;
        return false;
    }

    bool ret = false;
    if (reply->type == REDIS_REPLY_STRING)
    {
        connID = reply->str;
        ret = true;
    }
    else if (reply->type == REDIS_REPLY_NIL)
    {
        std::cerr << "User not found! " << std::endl;
        ret = true;
    }
    else if (reply->type == REDIS_REPLY_ERROR)
    {
        std::cerr << "Redis error: " << reply->str << std::endl;
    }
    else
    {
        std::cerr << "Unexpected Redis reply type: " << reply->type << std::endl;
    }

    freeReplyObject(reply);
    return ret; // 统一返回处理结果
}