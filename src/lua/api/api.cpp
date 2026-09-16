#include "./api.h"
#include <string>

namespace lua::api {
// Sanitization and security

// Escape html 
std::string APIs::escapeHTML(std::string s) {
  std::string out;

  for (char c : s) {
    if (c == '&')
      out += "&amp;";
    else if (c == '<')
      out += "&lt;";
    else if (c == '>')
      out += "&gt;";
    else
      out += c;
  }

  return out;
}
} // namespace lua::api