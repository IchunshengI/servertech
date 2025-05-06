#include <istream>
#include <string>
#include "config.h"
#include <iostream>
#include <fstream>
namespace chat{


Config& Config::Instance(){
  static Config config;
  return config;
}
/* 
  加载+解析配置文件
*/
bool Config::LoadConfigFile(const char* file_path){
  std::ifstream file(file_path);
  if(!file.is_open()){
    std::cerr << file_path << " does not exist or cannot be opened!" << std::endl;
    return false;
  }

  std::string line;
  while(std::getline(file, line)){
    Trim(line);

    /* 跳过空行和注释 */ 
    if(line.empty() || line[0] == '#'){
      continue;
    }

    /* 解析 */
    int idx = line.find('=');
    if (idx == -1) {
      continue;
    }

    std::string key;
    std::string value;
    key = line.substr(0,idx);
    Trim(key);
    value = line.substr(idx+1);
    Trim(value);

    config_map_.insert({key,value});
   
  }

  return true;
}


/* 
  查询配置项信息
*/
std::string Config::Load(const std::string& key){
  auto it = config_map_.find(key);
  if( it == config_map_.end()){
    return "";
  }
  return it->second;
}

/*
  去除多余空格
*/
void Config::Trim(std::string& src){
  const char* whitespace = " \t\n\r\f\v";  // 包含所有空白字符
  int idx = src.find_first_not_of(whitespace);
  if (idx != -1){
    /* 说明字符串前面有空格 */
    src = src.substr(idx, src.size()-idx);
  }

  /* 去掉后面多余空格 */
  idx = src.find_last_not_of(whitespace);
  if(idx !=-1){
    src = src.substr(0,idx+1);
  }
}


} // namespace chat