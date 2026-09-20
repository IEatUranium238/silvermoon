#ifndef CREATOR_H
#define CREATOR_H
#pragma once
#include "../../lua/manager/manager.h"
#include <map>
#include <pugixml.hpp>
#include <string>

namespace html::crt {
class Creator {
private:
  bool error = false;
  std::string escape(std::string s);
  void render(pugi::xml_node node, std::ostringstream &out,
              lua::mngr::LuaManager &script, std::string filename);

public:
  std::string createHTML(
      const pugi::xml_document &doc, std::string fp,
      std::map<std::string, std::string> cgi,
      std::map<std::string, std::string> headers, std::string body,
      std::map<std::string, std::string> &httpHeaders,
      std::map<std::string, std::variant<std::string, double, bool>> cookies);
};
} // namespace html::crt

#endif
