#ifndef MONGODB_CLIENT_h
#define MONGODB_CLIENT_h

#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>
#include <bsoncxx/builder/stream/document.hpp>

#include <mongocxx/client-fwd.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/collection-fwd.hpp>
#include <mongocxx/database-fwd.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/cursor.hpp>
#include <string>
#include <vector>

namespace chat {

class MonogodbClient
{

public:
  MonogodbClient(const std::string uri_string, const std::string db_name);

  /* 插入消息 */
  bool insertMessage(const int userID, const int sessionID,
                     const std::string& from, const std::string& content);

  bool insertSession(const int userID, const int sessionID);

  /* 查询消息 */
  std::vector<bsoncxx::document::value> queryMessages(const int userID,
                                                      const int sessionID);

private:
  mongocxx::instance instance_; /* 全局唯一客户端？*/  
  mongocxx::client client_;
  mongocxx::database db_;
  mongocxx::collection messages_; /* 消息集合*/      
  mongocxx::collection sessions_; /* 会话集合 */                                     
  
};

} // namespace chat

#endif