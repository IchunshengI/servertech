#include <iostream>
#include <chrono>
#include <string>
#include <boost/json.hpp>
#include "bigstringdata.pb.h"  // 生成的 Protobuf 头文件

using namespace std;
using namespace std::chrono;
namespace json = boost::json;

constexpr int N = 100000;

struct BigStringData {
    std::string id;
    std::string content;
};

struct BigStringSerializer {
    static tutorial::BigStringData to_protobuf(const BigStringData& data) {
        tutorial::BigStringData p;
        p.set_id(data.id);
        p.set_content(data.content);
        return p;
    }

    static BigStringData from_protobuf(const tutorial::BigStringData& p) {
        return {p.id(), p.content()};
    }

    static boost::json::value to_json(const BigStringData& data) {
        return {
            {"id", data.id},
            {"content", data.content}
        };
    }

    static BigStringData from_json(const boost::json::value& jv) {
        auto& obj = jv.as_object();
        return {
            boost::json::value_to<std::string>(obj.at("id")),
            boost::json::value_to<std::string>(obj.at("content"))
        };
    }
};

size_t test_protobuf(const BigStringData& data) {
    std::string buf;
    auto p = BigStringSerializer::to_protobuf(data);

    auto start = high_resolution_clock::now();
    for (int i = 0; i < N; ++i)
        p.SerializeToString(&buf);
    auto end = high_resolution_clock::now();
     cout << "Protobuf serialize: " << duration_cast<nanoseconds>(end - start).count() / N << " ns\n";

    tutorial::BigStringData parsed;
    start = high_resolution_clock::now();
    for (int i = 0; i < N; ++i)
        parsed.ParseFromString(buf);
    end = high_resolution_clock::now();
     cout << "Protobuf serialize: " << duration_cast<nanoseconds>(end - start).count() / N << " ns\n";

    start = high_resolution_clock::now();
    for (int i = 0; i < N; ++i)
        BigStringSerializer::from_protobuf(parsed);
    end = high_resolution_clock::now();
     cout << "Protobuf serialize: " << duration_cast<nanoseconds>(end - start).count() / N << " ns\n";

    return buf.size();
}

size_t test_json(const BigStringData& data) {
    auto jv = BigStringSerializer::to_json(data);
    std::string json_str;

    auto start = high_resolution_clock::now();
    for (int i = 0; i < N; ++i)
        json_str = json::serialize(jv);
    auto end = high_resolution_clock::now();
     cout << "Protobuf serialize: " << duration_cast<nanoseconds>(end - start).count() / N << " ns\n";

    json::value parsed;
    start = high_resolution_clock::now();
    for (int i = 0; i < N; ++i)
        parsed = json::parse(json_str);
    end = high_resolution_clock::now();
     cout << "Protobuf serialize: " << duration_cast<nanoseconds>(end - start).count() / N << " ns\n";

    start = high_resolution_clock::now();
    for (int i = 0; i < N; ++i)
        BigStringSerializer::from_json(parsed);
    end = high_resolution_clock::now();
     cout << "Protobuf serialize: " << duration_cast<nanoseconds>(end - start).count() / N << " ns\n";

    return json_str.size();
}

int main() {
    BigStringData data;
    data.id = "longstring001";

    // 生成一个超长字符串（比如1MB左右）
    data.content.resize(1024 * 4, 'a'); // 1MB 的 'a'

    cout << "--- Protobuf ---\n";
    size_t pb_size = test_protobuf(data);
    cout << "Size: " << pb_size << " bytes\n\n";

    cout << "--- JSON ---\n";
    size_t json_size = test_json(data);
    cout << "Size: " << json_size << " bytes\n";

    return 0;
}
