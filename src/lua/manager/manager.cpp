#include "./manager.h"
#include "../api/api.h"
#include "../transport/transport.h"
#include <filesystem>
#include <iostream>
#include <map>
#include <string_view>
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
    std::map<std::string, std::string> params, bool allowOpen, bool allowAdvFS,
    bool allowExecute, bool allowDynamicCode, lua::trpt::Transport &transport) {
  // Open libraries
  lua.open_libraries(sol::lib::base, sol::lib::coroutine, sol::lib::package,
                     sol::lib::string, sol::lib::os, sol::lib::math,
                     sol::lib::table, sol::lib::io, sol::lib::utf8,
                     sol::lib::bit32);

  // Kidnap evil functions from lua so user's skill issue wont blow up
  // production if someone is crazy enough to use it there
  lua["os"]["exit"] = sol::nil;
  lua["collectgarbage"] = sol::nil;

  if (!allowOpen) {
    lua["io"]["open"] = sol::nil;
  }

  if (!(allowOpen && allowAdvFS)) {
    lua["os"]["remove"] = sol::nil;
    lua["os"]["rename"] = sol::nil;
  }

  if (!allowDynamicCode) {
    lua["load"] = sol::nil;
    lua["loadstring"] = sol::nil;
    lua["dofile"] = sol::nil;
    lua["loadfile"] = sol::nil;
  }

  if (!allowExecute) {
    lua["os"]["execute"] = sol::nil;
    lua["io"]["popen"] = sol::nil;
  }

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

  // Transport prepare
  lua.script(serializer);

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

  lua["sm"]["VERSION"] = SM_VERSION;

  std::filesystem::path filePath(basePath);
  std::filesystem::path parentPath = filePath.parent_path();

  lua["sm"]["FOLDER"] = parentPath.string();

  // Request functions
  lua["sm"]["request"] = sol::as_table(cgi);
  lua["sm"]["header"] = sol::as_table(headers);
  lua["sm"]["body"] = body;
  lua["sm"]["params"] = sol::as_table(params);

  // Ugly get_form_contents related shit because it makes data shit itself for
  // no reasons if we treat it just like the others.
  struct FormCtx {
    std::string body;
    std::string type;
    lua::api::APIs api;
  };

  auto ctx = std::make_shared<FormCtx>();
  ctx->body = body;
  ctx->type = cgi["CONTENT_TYPE"];

  lua["sm"]["get_form_contents"] = [ctx]() {
    std::map<std::string, std::string> data;

    if (ctx->type == "application/x-www-form-urlencoded") {
      std::string_view query = ctx->body;
      while (!query.empty()) {
        size_t end = query.find('&');
        std::string_view param = query.substr(0, end);

        size_t equals = param.find('=');

        std::string name;
        std::string value;

        if (equals != std::string_view::npos) {
          name = std::string(param.substr(0, equals));
          value = std::string(param.substr(equals + 1));
        } else {
          // Parameters without = are treated as empty
          name = std::string(param);
        }

        // URL-decode
        name = ctx->api.unescapeURL(name);
        value = ctx->api.unescapeURL(value);

        data[name] = value;

        if (end == std::string_view::npos) {
          break;
        }

        query.remove_prefix(end + 1);
      }
    }

    if (ctx->type.starts_with("multipart/form-data")) {
      data =
          ctx->api.parseMultipart(ctx->body, ctx->api.getBoundary(ctx->type));
    }

    return sol::as_table(data);
  };

  // Security functions
  lua["sm"]["escape_html"] = [this](std::string str) {
    return api.escapeHTML(str);
  };

  lua["sm"]["unescape_html"] = [this](std::string str) {
    return api.unescapeHTML(str);
  };

  lua["sm"]["escape_url"] = [this](std::string str) {
    return api.escapeURL(str);
  };

  lua["sm"]["unescape_url"] = [this](std::string str) {
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

  // Transport
  lua["sm"]["transport"] = lua.create_table();
  lua["sm"]["transport"]["exists"] = [&transport](std::string name) {
    return transport.exists(name);
  };

  lua["sm"]["transport"]["delete"] = [&transport](std::string name) {
    return transport.remove(name);
  };

  // Internal c++ function which does all the dirty work
  lua["sm"]["transport"]["internal_dont_use_please_raw_set"] =
      [&transport, this](std::string name, sol::object data) -> std::string {
        sol::protected_function serialize =
            lua["sm_internal_DO_NOT_USE_IN_PROJECTS_serializer"];

        sol::protected_function_result result = serialize(data, name);
        std::string resErr = " ";

        if (!result.valid()) {
          sol::error err = result;
          resErr = "Failed to store '" + name + "': " + err.what();
          return resErr;
        }

        transport.set(name, result.get<std::string>());
        return resErr;
      };

  // Set user facing set API for error handeling
  lua.script(setAPI);

  lua["sm"]["transport"]["get"] = [&transport, this](std::string name) {
    std::string code = transport.get(name);

    return lua.script(code);
  };
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