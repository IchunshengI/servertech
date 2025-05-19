#include "mongodb_client.h"
#include "thread_poll.hpp"

int main() {
  


  std::string uri = "mongodb://tlx:hpcl6tlx@localhost:27017/?authSource=chat";
  chat::MonogodbClient mongodb_client(uri, "chat");
  
  chat::ThreadPool pool(4,8);
  pool.enqueue([&mongodb_client]()
  {
      auto messages = mongodb_client.queryMessages("alice", "session1");
      for (const auto& msg : messages) {
        std::cout << bsoncxx::to_json(msg) << std::endl;
    }
  });
  
  

  return 0;
}
