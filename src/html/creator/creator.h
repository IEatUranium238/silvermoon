#ifndef CREATOR_H
#define CREATOR_H
#pragma once
#include "../../lua/manager/manager.h"
#include "../../lua/transport/transport.h"
#include "../parser/parser.h"
#include <map>
#include <string>

namespace html::crt {
class Creator {
private:
  bool error = false;
  bool stopper = false;
public:
  std::string createHTML(
      std::vector<html::prs::Parser::parseItem> doc, std::string fp,
      std::map<std::string, std::string> cgi,
      std::map<std::string, std::string> headers, std::string body,
      std::map<std::string, std::string> &httpHeaders,
      std::map<std::string, std::variant<std::string, double, bool>> cookies,
      std::map<std::string, std::string> params, bool allowOpen,
      bool allowAdvFS, bool allowExecute, bool allowDynamicCode,
      lua::trpt::Transport &transport, std::map<std::string, std::string> formData);
};
} // namespace html::crt

#endif
