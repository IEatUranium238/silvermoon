#include "./manager.h"
#include <iostream>
#include <utility>

namespace lua::mngr {

// Escape characters for HTML
std::string escape(std::string s) {
  std::string out;

  for (char c : s) {
    if (c == '&')
      out += "&amp;";
    else if (c == '<')
      out += "&lt;";
    else if (c == '>')
      out += "&gt;";
    else
      out += c;
  }

  return out;
}

// Create a new lua manager
LuaManager::LuaManager(std::string basePath) {
  // Open all libraries
  lua.open_libraries(sol::lib::base, sol::lib::coroutine, sol::lib::package,
                     sol::lib::string, sol::lib::os, sol::lib::math,
                     sol::lib::table, sol::lib::io, sol::lib::debug,
                     sol::lib::utf8);

  // Add current file's path to module path
  if (!basePath.empty()) {
    std::string existingPath = lua["package"]["path"];
    std::string luaPath = basePath + "/?.lua;" + basePath + "/?/init.lua";
    lua["package"]["path"] = luaPath;
  }

  // Override print to act as echo
  lua["print"] = [this](sol::variadic_args va, sol::this_state ts) {
    for (auto arg : va) {
      std::string str = arg.as<std::string>();
      printed += str;
    }
  };
}

/// @brief Execute lua string code
/// @param code string to execute
/// @return [Status, Content] - Status (true = ok, false = fail), Content ( ok -
/// returned content, fail - error)
std::pair<bool, std::string> LuaManager::runCode(const std::string code) {
  auto result = lua.script(code, sol::script_pass_on_error);

  // Lua error
  if (!result.valid()) {
    sol::error err = result;

    std::string data = "<pre>";
    data += escape(err.what());
    data += "</pre>";

    return {false, data};
  }

  sol::object returnValue = result;

  // Set default resultStr to anything print outputed
  std::string resultStr = printed;
  printed = "";

  // Convert top level returned value to string
  if (returnValue.is<int>()) {
    resultStr += std::to_string(returnValue.as<int>());
  } else if (returnValue.is<double>()) {
    resultStr += std::to_string(returnValue.as<double>());
  } else if (returnValue.is<bool>()) {
    resultStr += returnValue.as<bool>() ? "true" : "false";
  } else if (returnValue.is<std::string>()) {
    resultStr += returnValue.as<std::string>();
  }

  return {true, resultStr};
}

} // namespace lua::mngr