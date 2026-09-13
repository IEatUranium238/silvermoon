#ifndef CREATOR_H
#define CREATOR_H
#pragma once
#include "../../lua/manager/manager.h"
#include <pugixml.hpp>
#include <string>

namespace html::crt {
class Creator {
private:
  bool error = false;
  std::string escape(std::string s);
  void render(pugi::xml_node node, std::ostringstream &out,
              lua::mngr::LuaManager &script);
  std::string beautifyHtml(const std::string &inputHtml);

public:
  std::string createHTML(const pugi::xml_document &doc, std::string fp);
};
} // namespace html::crt

#endif
