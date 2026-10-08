#ifndef TRANSPORT_H
#define TRANSPORT_H
#pragma once
#include <sol/sol.hpp>
#include <string>

namespace lua::trpt {
class Transport {
private:
  sol::state lua;
  std::string serializer = R"LUA(
function sm_internal_DO_NOT_USE_IN_PROJECTS_serializer(root)
    local function ser(v)
        local t = type(v)
        if t == "nil" or t == "boolean" then
            return tostring(v)
        elseif t == "number" or t == "string" then
            return string.format("%q", v)
        elseif t == "function" then
            return "load(" .. string.format("%q", string.dump(v)) .. ", '=imported', 'b')"
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
            error("Cannot serialize type: " .. t)
        end
    end

    return "return " .. ser(root)
end
)LUA";

public:
  Transport();
  bool exists(std::string name);
  void remove(std::string name);
  std::string get(std::string name);
  void set(std::string name, std::string code);
};
} // namespace lua::trpt
#endif
