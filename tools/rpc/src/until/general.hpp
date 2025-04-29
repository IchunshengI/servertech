/*
  Date	  :	2025年4月29日
  Author	:	chunsheng
  Brief		:	基本组件 / 通用函数的实现
*/

#include <array>
#include <openssl/rand.h>
#include <string>
#include <stdexcept>
#include "base64.hpp"
#pragma


namespace chat{

static constexpr std::size_t session_id_size = 16;
static std::string GenerateSessionToken()
{
  std::array<unsigned char, session_id_size> sid{};
  int ec = RAND_bytes(sid.data(), sid.size());
  if (ec <= 0)
      throw std::runtime_error("Generating session ID: RAND_bytes");

  return base64_encode(sid);
}

} // namespace chat
