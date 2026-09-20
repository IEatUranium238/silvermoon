#include "./manager.h"
#include "../api/api.h"
#include <filesystem>
#include <iostream>
#include <map>
#include <utility>

namespace lua::mngr {

// Escape characters for HTML
std::string LuaManager::escape(std::string s) {
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
    std::map<std::string, std::variant<std::string, double, bool>> cookies,
    std::map<std::string, std::string> params) {
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

  // Custom data types
  lua.new_usertype<CookieConfig>(
      "CookieConfig", sol::call_constructor,

      sol::factories([](sol::table config) {
        CookieConfig cookie;

        if (auto value = config["path"].get<sol::optional<std::string>>()) {
          cookie.path = *value;
        }

        if (auto value = config["domain"].get<sol::optional<std::string>>()) {
          cookie.domain = *value;
        }

        if (auto value = config["sameSite"].get<sol::optional<std::string>>()) {
          cookie.sameSite = *value;
        }

        if (auto value = config["secure"].get<sol::optional<bool>>()) {
          cookie.secure = *value;
        }

        if (auto value = config["httpOnly"].get<sol::optional<bool>>()) {
          cookie.httpOnly = *value;
        }

        if (auto value = config["partitioned"].get<sol::optional<bool>>()) {
          cookie.partitioned = *value;
        }

        if (auto value = config["maxAge"].get<sol::optional<int>>()) {
          cookie.maxAge = *value;
        }

        if (auto value = config["expires"].get<sol::optional<std::string>>()) {
          cookie.expires = *value;
        }

        if (auto value = config["host"].get<sol::optional<std::string>>()) {
          cookie.host = *value;
        }

        return cookie;
      }),

      "path", &CookieConfig::path, "domain", &CookieConfig::domain, "sameSite",
      &CookieConfig::sameSite, "secure", &CookieConfig::secure, "httpOnly",
      &CookieConfig::httpOnly, "partitioned", &CookieConfig::partitioned,
      "maxAge", &CookieConfig::maxAge, "expires", &CookieConfig::expires,
      "host", &CookieConfig::host);

  // API
  lua["sm"] = lua.create_table();
  lua::api::APIs api;

  lua["sm"]["VERSION"] = SM_VERSION;

  // Request functions
  lua["sm"]["request"] = sol::as_table(cgi);
  lua["sm"]["header"] = sol::as_table(headers);
  lua["sm"]["body"] = body;
  lua["sm"]["params"] = sol::as_table(params);

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
  lua["sm"]["set_cookie"] =
      [&cookies, &httpHeaders](std::string name,
                               std::variant<std::string, double, bool> content,
                               sol::optional<CookieConfig> cookieConfig) {
        cookies[name] = content;

        // Some cursed magic to convert variant to string
        std::string result = std::visit(
            [](const auto &arg) mutable {
              std::ostringstream oss;
              oss << arg;
              return oss.str();
            },
            content);

        std::string header = name + "=" + result;

        if (cookieConfig) {
          auto &config = *cookieConfig;

          if (config.path) {
            header += "; Path=" + *config.path;
          }

          if (config.domain) {
            header += "; Domain=" + *config.domain;
          }

          if (config.sameSite) {
            header += "; SameSite=" + *config.sameSite;
          }

          if (config.secure && *config.secure) {
            header += "; Secure";
          }

          if (config.httpOnly && *config.httpOnly) {
            header += "; HttpOnly";
          }

          if (config.partitioned && *config.partitioned) {
            header += "; Partitioned";
          }

          if (config.maxAge) {
            header += "; Max-Age=" + std::to_string(*config.maxAge);
          }

          if (config.expires) {
            header += "; Expires=" + *config.expires;
          }
        }

        httpHeaders["Set-Cookie"] = header;
      };

  lua["sm"]["delete_cookie"] = [&cookies, &httpHeaders](std::string name) {
    if (cookies.contains(name)) {
      cookies.erase(name);

      httpHeaders["Set-Cookie"] =
          name +
          "=deleted; Max-Age=0; Expires=Thu, 01 Jan 1970 00:00:00 GMT; Path=/";
    }
  };
}

/// @brief Execute lua string code
/// @param code string to execute
/// @return [Status, Content] - Status (true = ok, false = fail), Content ( ok -
/// returned content, fail - error)
std::pair<bool, std::string> LuaManager::runCode(const std::string code,
                                                 std::string filename) {
  auto result = lua.script(code, sol::script_pass_on_error, "@" + filename);

  std::cerr << result.valid() << std::endl;
  
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