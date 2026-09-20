#ifndef MANAGER_H
#define MANAGER_H
#pragma once
#include <map>
#include <sol/sol.hpp>
#include <sstream>
#include <string>
#include <utility>
#define SOL_ALL_SAFETIES_ON 1

namespace lua::mngr {
class LuaManager {
private:
  sol::state lua;
  std::string printed = "";

public:
  LuaManager(
      std::string basePath, std::map<std::string, std::string> cgi,
      std::map<std::string, std::string> headers, std::string body,
      std::map<std::string, std::string> &httpHeaders, bool &error,
      std::ostringstream &out,
      std::map<std::string, std::variant<std::string, double, bool>> cookies);
  std::pair<bool, std::string> runCode(const std::string code,
                                       std::string filename);
};
} // namespace lua::mngr
#endif
