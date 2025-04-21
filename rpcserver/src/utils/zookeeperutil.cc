#include "zookeeperutil.h"
#include "rpcapplication.h"
#include <semaphore.h>
#include "asynclogger.h"

/**
 * @brief 全局的观察器，当ZooKeeper服务端节点变更时，通知ZooKeeper客户端
 * @param zh ZooKeeper服务端句柄
 * @param type 回调的消息类型
 * @param state 回调的消息状态
 * @param watcherCtx 回调函数的参数
 */
void GlobalWatcher(zhandle_t *zh, int type, int state, const char *path, void *watcherCtx)
{
	if (type == ZOO_SESSION_EVENT)
	{
		if (state == ZOO_CONNECTED_STATE) // ZooKeeper服务端和客户端连接成功
		{
			sem_t *sem = (sem_t *)zoo_get_context(zh);
			sem_post(sem);
		}
	}
}

ZooKeeperClient::ZooKeeperClient()
	: zhandle_(nullptr) { zoo_set_debug_level(ZOO_LOG_LEVEL_ERROR); }

ZooKeeperClient::~ZooKeeperClient()
{
	if (zhandle_ != nullptr)
	{
		zookeeper_close(zhandle_);
	}
}

// 启动ZooKeeper客户端，连接ZooKeeper服务端
void ZooKeeperClient::Start()
{
	std::string host = RpcApplication::GetInstance().GetConfig().Load("zookeeperip");
	std::string port = RpcApplication::GetInstance().GetConfig().Load("zookeeperport");
	std::string connstr = host + ":" + port;

	zhandle_ = zookeeper_init(connstr.c_str(), GlobalWatcher, 30000, nullptr, nullptr, 0);
	if (nullptr == zhandle_)
	{
		LOG_ERROR << "ZooKeeper initial error!";
		exit(EXIT_FAILURE);
	}

	sem_t sem;
	sem_init(&sem, 0, 0);
	zoo_set_context(zhandle_, &sem);

	// 等待连接成功
	if (sem_wait(&sem) != 0)
	{
		LOG_ERROR << "Failed to wait for ZooKeeper connection!";
		exit(EXIT_FAILURE);
	}
	LOG_INFO << "ZooKeeper initial success!";
}

/**
 * @brief 在ZooKeeper服务端上根据指定的路径创建节点
 * @param path 节点路径
 * @param data 节点的值
 * @param state 节点类型
 */
void ZooKeeperClient::Create(const std::string &path, std::string data, int state)
{
	char path_buffer[128];
	int bufferlen = sizeof(path_buffer);
	int flag;
	flag = zoo_exists(zhandle_, path.c_str(), 0, nullptr);
	if (ZNONODE == flag)
	{
		flag = zoo_create(zhandle_,
						  path.c_str(),
						  data.c_str(),
						  data.size(),
						  &ZOO_OPEN_ACL_UNSAFE,
						  state,
						  path_buffer,
						  bufferlen);
		if (flag == ZOK)
		{
			LOG_INFO << "znode create success... path:" << path;
		}
		else
		{
			LOG_ERROR << "flag:" << flag;
			LOG_ERROR << "znode create error... path:" << path;
			exit(EXIT_FAILURE);
		}
	}
}


bool ZooKeeperClient::GetData(const std::string &path, std::string &data)
{
	char buffer[1024];
	int buffer_len = sizeof(buffer);
	struct Stat stat;

	// 调用 ZooKeeper API 获取数据
	int rc = zoo_get(zhandle_, path.c_str(), 0, buffer, &buffer_len, &stat);
	if (rc != ZOK)
	{
		// 处理错误
		return false;
	}
	// 拷贝数据到输出参数
	data.assign(buffer, buffer_len);

	return true;
}