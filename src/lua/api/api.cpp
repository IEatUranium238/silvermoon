#include "./api.h"
#include <cctype>
#include <iomanip>
#include <sstream>
#include <string>

namespace lua::api {
// Sanitization and security

// Escape html
std::string APIs::escapeHTML(std::string s) {
  std::string out;
  out.reserve(s.size());

  for (char c : s) {
    switch (c) {
    case '&':
      out += "&amp;";
      break;
    case '<':
      out += "&lt;";
      break;
    case '>':
      out += "&gt;";
      break;
    case '"':
      out += "&quot;";
      break;
    case '\'':
      out += "&#39;";
      break;
    default:
      out += c;
      break;
    }
  }

  return out;
}

std::string APIs::escapeURL(std::string input) {
  std::ostringstream out;
  out << std::hex << std::uppercase << std::setfill('0');

  for (unsigned char c : input) {
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out << static_cast<char>(c);
    } else if (c == ' ') {
      out << '+';
    } else {
      out << '%' << std::setw(2) << static_cast<int>(c);
    }
  }

  return out.str();
}

std::string APIs::unescapeURL(std::string input) {
  std::string out;
  out.reserve(input.size());

  for (std::size_t i = 0; i < input.size(); ++i) {
    char c = input[i];

    if (c == '+') {
      out += ' ';
    } else if (c == '%' && i + 2 < input.size() &&
               std::isxdigit(static_cast<unsigned char>(input[i + 1])) &&
               std::isxdigit(static_cast<unsigned char>(input[i + 2]))) {
      std::string hex = input.substr(i + 1, 2);
      char decoded = static_cast<char>(std::stoi(hex, nullptr, 16));
      out += decoded;
      i += 2;
    } else {
      out += c;
    }
  }

  return out;
}

} // namespace lua::api