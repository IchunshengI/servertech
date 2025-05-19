#include <bsoncxx/builder/stream/document-fwd.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/helpers.hpp>
#include <bsoncxx/types-fwd.hpp>
#include <mongocxx/client.hpp>
#include "mongodb_client.h"

namespace chat {


using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;

MonogodbClient::MonogodbClient(const std::string uri_string, const std::string db_name):
  client_{mongocxx::uri{uri_string}}, db_{client_[db_name]}
{
  messages_ = db_["messages"];
  sessions_ = db_["sessions"];
}


bool MonogodbClient::insertMessage(const std::string& user, const std::string& sessionID,
                     const std::string& from, const std::string& content)
{
  document doc_builder;
  doc_builder << "user" << user 
              << "sessionID" << sessionID
              << "from" << from
              << "text" << content
              << "ts" << bsoncxx::types::b_date{std::chrono::system_clock::now()};
  auto result = messages_.insert_one(doc_builder << finalize);
  return result.has_value();              
} 

bool MonogodbClient::insertSession(const std::string& user, const std::string& sessionID)
{
  document doc_builder;
  doc_builder << "user" << user
              << "sessionID" << sessionID;

  auto result = messages_.insert_one(doc_builder << finalize);
  return result.has_value();
}

  /* 查询消息 */
std::vector<bsoncxx::document::value> MonogodbClient::queryMessages(const std::string& user,
                                                      const std::string& sessionID)
{
  document filter;
  filter << "user" << user << "sessionID" << sessionID;

  std::vector<bsoncxx::document::value> result;
  auto cursor = messages_.find(filter.view());

  for( auto&& doc : cursor)
  {
    result.push_back(bsoncxx::document::value(doc));
  }

  return result;
}                                                      

} // namespace chat