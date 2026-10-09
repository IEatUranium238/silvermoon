#ifndef MANAGER_H
#define MANAGER_H
#pragma once
#include "../api/api.h"
#include "../transport/transport.h"
#include <map>
#include <sol/sol.hpp>
#include <sstream>
#include <string>
#include <utility>

namespace lua::mngr {
class LuaManager {
private:
  sol::state lua;
  std::string printed = "";
  std::string escape(std::string s);
  lua::api::APIs api;
  std::map<std::string, std::variant<std::string, double, bool>> cookiesCopy;

  struct CookieConfig {
    std::optional<std::string> path;
    std::optional<std::string> domain;
    std::optional<std::string> sameSite;

    std::optional<bool> secure;
    std::optional<bool> httpOnly;
    std::optional<bool> partitioned;

    std::optional<int> maxAge;
    std::optional<std::string> expires;
    std::optional<std::string> host;
  };

  std::string serializer = R"LUA(
function sm_internal_DO_NOT_USE_IN_PROJECTS_serializer(root)
    local function ser(v)
        local t = type(v)
        if t == "nil" or t == "boolean" then
            return tostring(v)
        elseif t == "number" or t == "string" then
            return string.format("%q", v)
        elseif t == "table" then
            local parts = {}

            for k, val in pairs(v) do
                parts[#parts + 1] = "[" .. ser(k) .. "]=" .. ser(val)
            end

            local code = "{" .. table.concat(parts, ",") .. "}"
            local mt = getmetatable(v)

            if mt then
                code = "setmetatable(" .. code .. ", " .. ser(mt) .. ")"
            end

            return code
        else
            error("cannot store type: " .. t, 0)
        end
    end

    return "return " .. ser(root)
end
)LUA";

  std::string setAPI = R"LUA(
  function sm.transport.set(name, data)
    local err = sm.transport.internal_dont_use_please_raw_set(name, data)
    if err ~= " " then error(err, 2) end
  end
)LUA";

public:
  LuaManager(   
      std::string basePath, std::map<std::string, std::string> cgi,
      std::map<std::string, std::string> headers, std::string body,
      std::map<std::string, std::string> &httpHeaders, bool &error,
      std::ostringstream &out,
      std::map<std::string, std::variant<std::string, double, bool>> cookies,
      std::map<std::string, std::string> params, bool allowOpen,
      bool allowAdvFS, bool allowExecute, bool allowDynamicCode,
      lua::trpt::Transport &transport, std::map<std::string, std::string> formData);
  std::pair<bool, std::string> runCode(const std::string code,
                                       std::string filename);
};
} // namespace lua::mngr

#endif
