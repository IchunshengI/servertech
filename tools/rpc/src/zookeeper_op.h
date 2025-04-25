#ifndef ZOOKEEPER_OP_H
#define ZOOKEEPER_OP_H

#include <memory>
#include <semaphore.h>
#include <zookeeper/zookeeper.h>
#include <string>

namespace rpc{
/* 封装的zk客户端 */
class ZkClient{

public:
  ZkClient();
  ~ZkClient();

  void Start();
  void Create(const char *path, const char* data, int datalen, int state=0);
  std::string GetData(const char *path,watcher_fn watcher_cb, void* context);
  static void server_watcher(zhandle_t *zh, int type,
                    int state, const char* path __attribute__((unused)), 
                    void *watcherCtx __attribute__((unused))
                    );
private:
  sem_t sem_;
  zhandle_t* zhandle_;
};

} // namespace rpc
#endif