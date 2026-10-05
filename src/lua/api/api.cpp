#include "./api.h"
#include <cctype>
#include <iomanip>
#include <map>
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
    default:
      out += c;
      break;
    }
  }

  return out;
}

// Escape Attribute
std::string APIs::escapeAttribute(std::string s) {
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

// Unescape HTML
std::string APIs::unescapeHTML(std::string s) {
  std::string out;
  out.reserve(s.size());

  for (size_t i = 0; i < s.size(); ++i) {
    if (s.compare(i, 5, "&amp;") == 0) {
      out += '&';
      i += 4;
    } else if (s.compare(i, 4, "&lt;") == 0) {
      out += '<';
      i += 3;
    } else if (s.compare(i, 4, "&gt;") == 0) {
      out += '>';
      i += 3;
    } else {
      out += s[i];
    }
  }

  return out;
}

// Unescape Attribute
std::string APIs::unescapeAttribute(std::string s) {
  std::string out;
  out.reserve(s.size());

  for (size_t i = 0; i < s.size(); ++i) {
    if (s.compare(i, 5, "&amp;") == 0) {
      out += '&';
      i += 4;
    } else if (s.compare(i, 4, "&lt;") == 0) {
      out += '<';
      i += 3;
    } else if (s.compare(i, 4, "&gt;") == 0) {
      out += '>';
      i += 3;
    } else if (s.compare(i, 6, "&quot;") == 0) {
      out += '"';
      i += 5;
    } else if (s.compare(i, 5, "&#39;") == 0) {
      out += '\'';
      i += 4;
    } else {
      out += s[i];
    }
  }

  return out;
}

// Escape url
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

// Unescape url
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

// multipart parsing

std::string APIs::getBoundary(std::string content_type) {
  std::string key = "boundary=";

  size_t pos = content_type.find(key);

  if (pos == std::string::npos) {
    return "";
  }

  std::string b = content_type.substr(pos + key.size());

  if (!b.empty() && b[0] == '"') {
    size_t end = b.find('"', 1);
    return b.substr(1, end == std::string::npos ? end : end - 1);
  }

  return b.substr(0, b.find(';'));
}

std::map<std::string, std::string> APIs::parseMultipart(std::string body,
                                                        std::string boundary) {
  std::map<std::string, std::string> result;
  std::string div = "--" + boundary;

  size_t pos = body.find(div);
  while (pos != std::string::npos) {
    pos += div.size();

    if (body.compare(pos, 2, "--") == 0) {
      break;
    }

    pos += 2; // skip CRLF

    size_t hEnd = body.find("\r\n\r\n", pos);

    if (hEnd == std::string::npos) {
      break;
    }

    std::string headers = body.substr(pos, hEnd - pos);

    size_t dataStart = hEnd + 4;
    size_t next = body.find("\r\n" + div, dataStart);

    if (next == std::string::npos) {
      break;
    }

    // Name fields
    std::string key = "; name=\"";
    size_t n = headers.find(key);

    if (n != std::string::npos) {
      n += key.size();
      size_t nEnd = headers.find('"', n);

      if (nEnd != std::string::npos) {
        result[headers.substr(n, nEnd - n)] =
            body.substr(dataStart, next - dataStart);
      }
    }

    pos = next + 2; // Continue to next
  }
  return result;
}

} // namespace lua::api