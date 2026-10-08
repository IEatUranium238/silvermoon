#include "./transport.h"
#include <string>

namespace lua::trpt {
Transport::Transport() {
  // Open libraries
  lua.open_libraries(sol::lib::base, sol::lib::coroutine, sol::lib::package,
                     sol::lib::string, sol::lib::os, sol::lib::math,
                     sol::lib::table, sol::lib::io, sol::lib::utf8,
                     sol::lib::bit32);

  lua.script(serializer);
}

bool Transport::exists(std::string name) {
  return (lua[name].valid() && lua[name].get_type() != sol::type::nil) ? true : false;
}

void Transport::remove(std::string name) { lua[name] = sol::nil; }

void Transport::set(std::string name, std::string code) {
  lua[name] = lua.script(code);
}

std::string Transport::get(std::string name) {
  sol::function serialize =
      lua["sm_internal_DO_NOT_USE_IN_PROJECTS_serializer"];
  std::string code = serialize(lua[name]);

  return code;
}

} // namespace lua::trpt