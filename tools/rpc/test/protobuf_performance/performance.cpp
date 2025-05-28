#include <iostream>
#include <chrono>
#include <string>
#include <vector>
#include <boost/json.hpp>
#include "addressbook.pb.h"  // 编译后的 Protobuf 文件

using namespace std;
using namespace std::chrono;
namespace json = boost::json;

constexpr int N = 100000;

struct Phone {
    std::string number;
    int type;
};

struct PersonData {
    std::string name;
    int id;
    std::string email;
    std::vector<Phone> phones;
};


struct PersonSerializer {
    static tutorial::Person to_protobuf(const PersonData& person) {
        tutorial::Person p;
        p.set_name(person.name);
        p.set_id(person.id);
        p.set_email(person.email);
        for (const auto& ph : person.phones) {
            auto* pb = p.add_phones();
            pb->set_number(ph.number);
            pb->set_type(ph.type);
        }
        return p;
    }

    static PersonData from_protobuf(const tutorial::Person& p) {
        PersonData pd;
        pd.name = p.name();
        pd.id = p.id();
        pd.email = p.email();
        for (const auto& ph : p.phones()) {
            pd.phones.push_back({ph.number(), ph.type()});
        }
        return pd;
    }

    static boost::json::value to_json(const PersonData& person) {
        boost::json::array phone_arr;
        for (const auto& ph : person.phones) {
            phone_arr.push_back({{"number", ph.number}, {"type", ph.type}});
        }
        return {
            {"name", person.name},
            {"id", person.id},
            {"email", person.email},
            {"phones", phone_arr}
        };
    }

    static PersonData from_json(const boost::json::value& jv) {
        PersonData pd;
        const auto& obj = jv.as_object();
        pd.name = boost::json::value_to<std::string>(obj.at("name"));
        pd.id = boost::json::value_to<int>(obj.at("id"));
        pd.email = boost::json::value_to<std::string>(obj.at("email"));
        for (const auto& ph : obj.at("phones").as_array()) {
            const auto& ph_obj = ph.as_object();
            pd.phones.push_back({
                boost::json::value_to<std::string>(ph_obj.at("number")),
                boost::json::value_to<int>(ph_obj.at("type"))
            });
        }
        return pd;
    }
};

size_t test_protobuf(const PersonData& person) {
    std::string buf;
    auto p = PersonSerializer::to_protobuf(person);

    auto start = high_resolution_clock::now();
    for (int i = 0; i < N; ++i)
        p.SerializeToString(&buf);
    auto end = high_resolution_clock::now();
    cout << "Protobuf serialize: " << duration_cast<microseconds>(end - start).count() / N << " us\n";

    tutorial::Person parsed;
    start = high_resolution_clock::now();
    for (int i = 0; i < N; ++i)
        parsed.ParseFromString(buf);
    end = high_resolution_clock::now();
    cout << "Protobuf parse: " << duration_cast<microseconds>(end - start).count() / N << " us\n";

    start = high_resolution_clock::now();
    for (int i = 0; i < N; ++i)
        PersonSerializer::from_protobuf(parsed);
    end = high_resolution_clock::now();
    cout << "Protobuf to class: " << duration_cast<microseconds>(end - start).count() / N << " us\n";

    return buf.size();
}

size_t test_json(const PersonData& person) {
    auto jv = PersonSerializer::to_json(person);
    std::string json_str;

    auto start = high_resolution_clock::now();
    for (int i = 0; i < N; ++i)
        json_str = json::serialize(jv);
    auto end = high_resolution_clock::now();
    cout << "JSON serialize: " << duration_cast<microseconds>(end - start).count() / N << " us\n";

    json::value parsed;
    start = high_resolution_clock::now();
    for (int i = 0; i < N; ++i)
        parsed = json::parse(json_str);
    end = high_resolution_clock::now();
    cout << "JSON parse: " << duration_cast<microseconds>(end - start).count() / N << " us\n";

    start = high_resolution_clock::now();
    for (int i = 0; i < N; ++i)
        PersonSerializer::from_json(parsed);
    end = high_resolution_clock::now();
    cout << "JSON to class: " << duration_cast<microseconds>(end - start).count() / N << " us\n";

    return json_str.size();
}

int main() {
    PersonData person;
    person.name = "xiaowang";
    person.id = 1;
    person.email = "123@qq.com";
    for (int i = 0; i < 500; ++i)
        person.phones.push_back({"156888888" + std::to_string(i), i % 3});

    cout << "--- Protobuf ---\n";
    size_t pb_size = test_protobuf(person);
    cout << "Size: " << pb_size << " bytes\n\n";

    cout << "--- JSON ---\n";
    size_t json_size = test_json(person);
    cout << "Size: " << json_size << " bytes\n";
}
