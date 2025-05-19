#include <iostream>
#include <chrono>

#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>
#include <bsoncxx/builder/stream/document.hpp>

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/cursor.hpp>

int main() {
    try {
        // 必须有一个 instance（整个程序中只能有一个）
        mongocxx::instance instance{};

        // 创建连接
        mongocxx::client client{mongocxx::uri{"mongodb://tlx:hpcl6tlx@localhost:27017/?authSource=chat"}};

        // 获取数据库和集合
        mongocxx::database db = client["chat"];
        mongocxx::collection coll = db["messages"];

        {
          auto sessions = db["sessions"];
          auto messages = db["messages"];

          // 创建联合索引: { user: 1, sessionID: 1 }
          bsoncxx::builder::stream::document index_builder;
          index_builder << "user" << 1 << "sessionID" << 1;

          sessions.create_index(index_builder.view());
          messages.create_index(index_builder.view());

        }

        // 查询指定 user 和 sessionID 的数据
        std::cout << "Initial Query for user 'user1' and sessionID 'session1':\n";
        bsoncxx::builder::stream::document filter_builder;
        filter_builder << "user" << "user1" << "sessionID" << "session1";

        auto cursor = coll.find(filter_builder.view());
        for (auto&& doc : cursor) {
            std::cout << bsoncxx::to_json(doc) << std::endl;
        }

        // 插入一条数据~  
        {
        std::cout << "\nInserting a new message for user 'alice' and sessionID 'session1':\n";
        bsoncxx::builder::stream::document document_builder;
        document_builder
            << "user" << "alice"
            << "sessionID" << "session1"
            << "from" << "user1"
            << "text" << "hello alice"
            << "ts" << bsoncxx::types::b_date{std::chrono::system_clock::now()};

        bsoncxx::document::value doc_value = document_builder << bsoncxx::builder::stream::finalize;
        bsoncxx::stdx::optional<mongocxx::result::insert_one> result = coll.insert_one(doc_value.view());

        if (result) {
            std::cout << "Inserted with ID: " << result->inserted_id().get_oid().value.to_string() << std::endl;
        } else {
            std::cerr << "Insert failed." << std::endl;
        }

        }


        {
        auto coll = db["sessions"];
        std::cout << "\nInserting a new message for user 'alice' and sessionID 'session1':\n";
        bsoncxx::builder::stream::document document_builder;
        document_builder
            << "user" << "alice"
            << "sessionID" << "session1";
            // << "startedAt" << bsoncxx::types::b_date{std::chrono::system_clock::now()}
            // << "recentMessages" << "";

        bsoncxx::document::value doc_value = document_builder << bsoncxx::builder::stream::finalize;
        bsoncxx::stdx::optional<mongocxx::result::insert_one> result = coll.insert_one(doc_value.view());

        if (result) {
            std::cout << "Inserted with ID: " << result->inserted_id().get_oid().value.to_string() << std::endl;
        } else {
            std::cerr << "Insert failed." << std::endl;
        }

        }
        // 查询 Alice 的数据
        std::cout << "\nQuery for user 'alice' and sessionID 'session1' after insertion:\n";
        bsoncxx::builder::stream::document filter_alice;
        filter_alice << "user" << "alice" << "sessionID" << "session1";

        auto cursor_alice = coll.find(filter_alice.view());
        for (auto&& doc : cursor_alice) {
            std::cout << bsoncxx::to_json(doc) << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
