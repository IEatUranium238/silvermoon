#ifndef API_H
#define API_H
#pragma once
#include <string>

namespace lua::api {
class APIs {
public:
  std::string escapeHTML(std::string s);
};
} // namespace lua::mngr
#endif
