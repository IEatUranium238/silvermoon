#include "./transport.h"
#include <mutex>
#include <string>

namespace lua::trpt {
Transport::Transport() {
  lua.open_libraries(sol::lib::base, sol::lib::coroutine, sol::lib::package,
                     sol::lib::string, sol::lib::os, sol::lib::math,
                     sol::lib::table, sol::lib::io, sol::lib::utf8,
                     sol::lib::bit32);

  lua.script(serializer);
}

bool Transport::exists(std::string name) {
  std::lock_guard<std::mutex> lock(mtx);

  sol::object obj = lua[name];
  return obj.valid() && obj.get_type() != sol::type::nil;
}

void Transport::remove(std::string name) {
  std::lock_guard<std::mutex> lock(mtx);

  lua[name] = sol::nil;
}

void Transport::set(std::string name, std::string code) {
  std::lock_guard<std::mutex> lock(mtx);

  lua[name] = lua.script(code);
}

std::string Transport::get(std::string name) {
  std::lock_guard<std::mutex> lock(mtx);

  sol::function serialize =
      lua["sm_internal_DO_NOT_USE_IN_PROJECTS_serializer"];
  std::string code = serialize(lua[name]);
  return code;
}

} // namespace lua::trpt