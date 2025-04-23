#include <semaphore.h>
#include <zookeeper/zookeeper.h>
#include "zookeeper_op.h"
#include "config/config.h"
#include "log/logger_wrapper.h"

namespace rpc {
using chat::LOG;


/* zkserver给zkclient的通知接收 */
void ZkClient::server_watcher(zhandle_t *zh, int type,
                    int state, const char* path __attribute__((unused)), 
                    void *watcherCtx __attribute__((unused))
                    )
{
  if (type == ZOO_SESSION_EVENT){
    /* 初始化连接状态 */
    if (state == ZOO_CONNECTED_STATE){
  //    ZkClient* client = static_cast<ZkClient*>(watcherCtx);
      ZkClient * zk_t = (ZkClient*)zoo_get_context(zh);
      sem_post(&zk_t->sem_);
    } 
  }
}


/* 
else if (state == ZOO_EXPIRED_SESSION_STATE){
      LOG("Debug") << "Session expired, reconnection";
      ZkClient * zk_t = (ZkClient*)zoo_get_context(zh);
      zk_t->Start();
    }
*/
/* 连接成功的标志 */
void ZkClient::Start()
{
  std::string host = chat::Config::Instance().Load("zookeeperIp");
  std::string port = chat::Config::Instance().Load("zookeeperPort");
  std::string connstr = host + ":" + port;

  zhandle_ = zookeeper_init(connstr.c_str(), ZkClient::server_watcher, 
                            30000, nullptr, this, 0);
  if (zhandle_ == nullptr){
    LOG("Error") << "zookeeper init error!";
    return;
  }

  sem_init(&sem_, 0, 0);
  //zoo_set_context(zhandle_,&sem_);
  sem_wait(&sem_);
}


void ZkClient::Create(const char *path, const char* data, int datalen, int state)
{
    char path_buffer[128];
    int bufferlen = sizeof(path_buffer);
    int flag;
	  // 先判断path表示的znode节点是否存在，如果存在，就不再重复创建了
  	flag = zoo_exists(zhandle_, path, 0, nullptr);
  	if (ZNONODE == flag) // 表示path的znode节点不存在
  	{
  		// 创建指定path的znode节点了
  		flag = zoo_create(zhandle_, path, data, datalen,
  			&ZOO_OPEN_ACL_UNSAFE, state, path_buffer, bufferlen);
  		if (flag == ZOK)
  		{
  			LOG("Debug")<< "znode create success... path:" << path ;
  		}
  		else
  		{
  			LOG("Debug")<< "flag:" << flag ;
  			LOG("Debug")<< "znode create error... path:" << path ;
  			exit(EXIT_FAILURE);
  		}
  	}    

}
std::string ZkClient::GetData(const char *path, watcher_fn watcher_cb, void* context)
{
	String_vector children;
//int flag = zoo_get_children(zhandle_, path, 0, &children);
  int flag = zoo_wget_children(zhandle_, path, watcher_cb, context, &children);
// zoo_wget_children
  if (flag != ZOK) {
      LOG("Error") << "Failed to get children of " << path << std::endl;
      return std::string();
  }

// 2. 遍历子节点获取数据
  for (int i = 0; i < children.count; ++i) {
    std::string child_path = std::string(path) + "/" + children.data[i];
	  LOG("Debug")<< "child_path is << " << child_path;
    char buffer[128];
    int buffer_len = sizeof(buffer);
    flag = zoo_get(zhandle_, child_path.c_str(), 0, buffer, &buffer_len, nullptr);
	  if (flag == ZOK) {
         // instances.push_back(std::string(buffer, buffer_len));
	   	  return buffer;
    } else {
        LOG("Error") << "Failed to get data from: " << child_path << " Error: " << flag;
		    return std::string();
      }
  }
  return std::string();
}


ZkClient::ZkClient() : zhandle_(nullptr)
{

}

ZkClient::~ZkClient()
{
  if (zhandle_ != nullptr){
    zookeeper_close(zhandle_);
  }
}
} // namespace rpc