#ifndef MANAGER_H
#define MANAGER_H
#pragma once
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
  LuaManager(std::string basePath);
  std::pair<bool, std::string> runCode(const std::string code);
};
} // namespace lua::mngr
#endif
