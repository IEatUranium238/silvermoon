#ifndef API_H
#define API_H
#pragma once
#include <string>

namespace lua::api {
class APIs {
public:
  std::string escapeHTML(std::string s);
  std::string escapeURL(std::string input);
  std::string unescapeURL(std::string input);
};
} // namespace lua::mngr
#endif
