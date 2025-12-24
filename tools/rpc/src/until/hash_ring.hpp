/* 
    一致性哈希
*/

#pragma once
#include <map>
#include <string>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include "xxhash.h"
namespace rpc {

class ConsistentHashRing{
    public:
        explicit ConsistentHashRing(int virtual_nodes = 100) : virtual_nodes_(virtual_nodes){}

        void Build(const std::vector<std::string>& nodes)
        {
            ring_.clear();
            for(const auto& node : nodes)
            {
                // 添加节点
                AddNode(node);
            }
        }

        // 根据key选择节点
        std::string GetNode(const std::string& key) const{
            if(ring_.empty()){
                return std::string();
            }

            uint64_t hash = XXH64(key.data(), key.size(), 0);
            auto it = ring_.lower_bound(hash);
            if(it == ring_.end()){
                it = ring_.begin(); // 回环
            }
            return it->second;
        }

        bool Empty() const{
            return ring_.empty();
        }

    private:

        void AddNode(const std::string& node){

            // endpoint先hash一次
            uint64_t base = XXH64(node.data(), node.size(), 0);
            // 虚拟节点，把idx混进去
            for(int i = 0; i < virtual_nodes_; i++)
            {
                uint64_t hash = XXH64(&i, sizeof(i), base);
                ring_[hash] = node;
            }
        }

        int virtual_nodes_;
        std::map<uint64_t, std::string> ring_;
};

}