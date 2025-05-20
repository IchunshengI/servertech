#include "mongodb_client.h"
#include "thread_poll.hpp"

int main() {
  

  std::string uri = "mongodb://tlx:hpcl6tlx@localhost:27017/?authSource=chat";
  chat::MonogodbClient mongodb_client(uri, "chat");
  

  //mongodb_client.insertMessage(1, 1, "user", "hello");
  mongodb_client.insertSession(1,1);
  // chat::ThreadPool pool(4,8);
  // pool.enqueue([&mongodb_client]()
  // {
  //     auto messages = mongodb_client.queryMessages(1, 1);
  //     for (const auto& msg : messages) {
  //       std::cout << bsoncxx::to_json(msg) << std::endl;
  //   }
  // });
  
  

  return 0;
}
