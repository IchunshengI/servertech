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
        auto db = client["chat"];
        auto coll = db["messages"];

        // 查询指定 user 和 sessionKey 的数据
        std::cout << "Initial Query for user 'user1' and sessionKey 'session1':\n";
        bsoncxx::builder::stream::document filter_builder;
        filter_builder << "user" << "user1" << "sessionKey" << "session1";

        auto cursor = coll.find(filter_builder.view());
        for (auto&& doc : cursor) {
            std::cout << bsoncxx::to_json(doc) << std::endl;
        }

        // 插入一条数据
        std::cout << "\nInserting a new message for user 'alice' and sessionKey 'session1':\n";
        bsoncxx::builder::stream::document document_builder;
        document_builder
            << "user" << "alice"
            << "sessionKey" << "session1"
            << "from" << "user1"
            << "content" << "hello alice"
            << "timestamp" << bsoncxx::types::b_date{std::chrono::system_clock::now()};

        bsoncxx::document::value doc_value = document_builder << bsoncxx::builder::stream::finalize;
        bsoncxx::stdx::optional<mongocxx::result::insert_one> result = coll.insert_one(doc_value.view());

        if (result) {
            std::cout << "Inserted with ID: " << result->inserted_id().get_oid().value.to_string() << std::endl;
        } else {
            std::cerr << "Insert failed." << std::endl;
        }

        // 查询 Alice 的数据
        std::cout << "\nQuery for user 'alice' and sessionKey 'session1' after insertion:\n";
        bsoncxx::builder::stream::document filter_alice;
        filter_alice << "user" << "alice" << "sessionKey" << "session1";

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
