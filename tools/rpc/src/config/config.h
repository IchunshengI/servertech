/*
  Date	  :	2025年4月20日
  Author	:	chunsheng
  Brief		:	配置文件加载操作类，用于直接从配置文件加载对应数据避免在程序中写死
*/


#ifndef CONFIG_H
#define CONFIG_H

#include <unordered_map>
#include <string>

namespace chat{

class Config{
 public:

  Config(const Config&) = delete;
  static Config& Instance();

  /* 解析和加载配置文件 */
  bool LoadConfigFile(const char* file_path);
  /* 查询配置项信息 */
  std::string Load(const std::string& key);

 private:
  Config() = default; 
   /* 兼容性，去除前后空格 */
  void Trim(std::string &src);
  std::unordered_map<std::string, std::string> config_map_;

};


} // namespace chat


#endif