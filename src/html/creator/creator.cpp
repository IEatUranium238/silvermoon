#include "./creator.h"
#include "../../lua/manager/manager.h"
#include "../parser/parser.h"
#include <algorithm>
#include <map>
#include <sstream>
#include <string>
#include <unordered_set>

namespace html::crt {

std::string Creator::createHTML(
    std::vector<html::prs::Parser::parseItem> doc, std::string fp,
    std::map<std::string, std::string> cgi,
    std::map<std::string, std::string> headers, std::string body,
    std::map<std::string, std::string> &httpHeaders,
    std::map<std::string, std::variant<std::string, double, bool>> cookies,
    std::map<std::string, std::string> params, bool allowOpen, bool allowAdvFS,
    bool allowExecute, bool allowDynamicCode, lua::trpt::Transport &transport,
    std::map<std::string, std::string> formData) {
  std::ostringstream out;
  lua::mngr::LuaManager script(fp, cgi, headers, body, httpHeaders, stopper,
                               out, cookies, params, allowOpen, allowAdvFS,
                               allowExecute, allowDynamicCode, transport,
                               formData);

  error = false;
  stopper = false;

  for (html::prs::Parser::parseItem item : doc) {
    if (stopper || error) {
      break;
    }

    // Raw html
    if (!item.isLua) {
      out << item.content;
      continue;
    }

    // Lua
    auto [success, result] = script.runCode(item.content, cgi["SCRIPT_NAME"]);

    if (!success) {
      error = true;
      out << "<p><b>[LUA ERROR!]</b></p>";
    }

    out << result;
  }

  return out.str();
}
} // namespace html::crt