#include "config.h"
#include <iostream>

int main(int argc, char **argv){

  const char* file = "config_test.cfg";

  chat::Config config;
  config.LoadConfigFile(file);
  std::string ss = config.Load("tickTime");

  if(!ss.empty()){
    std::cout << "tickTime value is " << ss << std::endl;
  }else{
    std::cout << "tickTime value load error" << std::endl;
  }
  return 0;
}