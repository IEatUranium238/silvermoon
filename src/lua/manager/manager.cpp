#include "./manager.h"
#include "../api/api.h"
#include <filesystem>
#include <iostream>
#include <map>
#include <utility>

namespace lua::mngr {

// Escape characters for HTML
std::string escape(std::string s) {
  std::string out;
  out.reserve(s.size());

  for (char c : s) {
    switch (c) {
    case '&':
      out += "&amp;";
      break;
    case '<':
      out += "&lt;";
      break;
    case '>':
      out += "&gt;";
      break;
    case '"':
      out += "&quot;";
      break;
    case '\'':
      out += "&#39;";
      break;
    default:
      out += c;
      break;
    }
  }

  return out;
}

// Create a new lua manager
LuaManager::LuaManager(
    std::string basePath, std::map<std::string, std::string> cgi,
    std::map<std::string, std::string> headers, std::string body,
    std::map<std::string, std::string> &httpHeaders, bool &error,
    std::ostringstream &out,
    std::map<std::string, std::variant<std::string, double, bool>> cookies) {
  // Open libraries
  lua.open_libraries(sol::lib::base, sol::lib::coroutine, sol::lib::package,
                     sol::lib::string, sol::lib::os, sol::lib::math,
                     sol::lib::table, sol::lib::io, sol::lib::utf8,
                     sol::lib::bit32);

  // Kidnap evil functions from lua so user's skill issue wont blow up
  // production if someone is crazy enough to use it there
  lua["os"]["execute"] = sol::nil;
  lua["os"]["exit"] = sol::nil;
  lua["os"]["remove"] = sol::nil;
  lua["os"]["rename"] = sol::nil;

  lua["io"]["open"] = sol::nil;
  lua["io"]["popen"] = sol::nil;

  lua["dofile"] = sol::nil;
  lua["loadfile"] = sol::nil;

  // Add current file's path to module path
  if (!basePath.empty()) {
    std::filesystem::path filePath(basePath);
    std::filesystem::path parentPath = filePath.parent_path();

    std::string existingPath = lua["package"]["path"];
    std::string luaPath =
        parentPath.string() + "/?.lua;" + parentPath.string() + "/?/init.lua";

    lua["package"]["path"] = luaPath + ";" + existingPath;
  }

  // Override print to act as echo
  lua["print"] = [this](sol::variadic_args va, sol::this_state ts) {
    sol::state_view lua(ts);

    sol::function tostring = lua["tostring"];

    bool first = true;

    for (auto arg : va) {
      if (!first)
        printed += '\t';

      first = false;

      sol::protected_function_result result = tostring(arg);

      printed += result.get<std::string>();
    }

    printed += '\n';
  };

  // API
  lua["sm"] = lua.create_table();
  lua::api::APIs api;

  // Request functions
  lua["sm"]["request"] = sol::as_table(cgi);
  lua["sm"]["header"] = sol::as_table(headers);
  lua["sm"]["body"] = body;

  // Security functions
  lua["sm"]["escape_html"] = [&api](std::string str) {
    return api.escapeHTML(str);
  };
  lua["sm"]["unescape_html"] = [&api](std::string str) {
    return api.unescapeHTML(str);
  };

  lua["sm"]["escape_url"] = [&api](std::string str) {
    return api.escapeURL(str);
  };

  lua["sm"]["unescape_url"] = [&api](std::string str) {
    return api.unescapeURL(str);
  };

  // Reponse functions
  lua["sm"]["set_http_code"] = [&httpHeaders](int newCode) {
    httpHeaders["Status"] = std::to_string(newCode);
  };

  lua["sm"]["set_mime_type"] = [&httpHeaders](std::string newMime) {
    httpHeaders["Content-Type"] = newMime;
  };

  lua["sm"]["set_header"] = [&httpHeaders](std::string headerName,
                                           std::string headerContent) {
    httpHeaders[headerName] = headerContent;
  };

  lua["sm"]["delete_header"] = [&httpHeaders](std::string headerName) {
    httpHeaders.erase(headerName);
  };

  lua["sm"]["redirect"] = [&httpHeaders](std::string location) {
    httpHeaders["Status"] = "302";
    httpHeaders["Location"] = location;
  };

  lua["sm"]["halt"] = [&error]() { error = true; };
  lua["sm"]["set_page_content"] = [&out, &error](std::string newContent) {
    error = true;
    out.str("");
    out.clear();
    out << newContent;
  };

  // Cookies
  lua["sm"]["cookies"] = sol::as_table(cookies);
}

/// @brief Execute lua string code
/// @param code string to execute
/// @return [Status, Content] - Status (true = ok, false = fail), Content ( ok -
/// returned content, fail - error)
std::pair<bool, std::string> LuaManager::runCode(const std::string code,
                                                 std::string filename) {
  auto result = lua.script(code, sol::script_pass_on_error, "@" + filename);

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