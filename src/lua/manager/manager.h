#ifndef MANAGER_H
#define MANAGER_H
#pragma once
#include <map>
#include <sol/sol.hpp>
#include <string>
#include <utility>
#define SOL_ALL_SAFETIES_ON 1

namespace lua::mngr {
class LuaManager {
private:
  sol::state lua;
  std::string printed = "";

public:
  LuaManager(std::string basePath, std::map<std::string, std::string> cgi,
             std::map<std::string, std::string> headers,
             std::map<std::string, std::string> body);
  std::pair<bool, std::string> runCode(const std::string code);
};
} // namespace lua::mngr
#endif
