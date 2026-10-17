#include <algorithm>
#include <cctype>
#include <fstream>
#include <string>
#include <vector>

#include "./parser.h"

namespace html::prs {

// RIP "CAUTION: This shit is ugly as fuck" comment
// It died during the refactoring.
//
// 20.9.2026 - 17.10.2026, you will be missed
// "Keep small children and pets away from it, IT WILL EAT THEM"

void parseContents(std::string contents,
                   std::vector<Parser::parseItem> &parsedContent) {
  size_t n = contents.size();

  // lowercase it for matching
  std::string lower = contents;
  for (size_t k = 0; k < n; k += 1) {
    lower[k] = std::tolower(static_cast<unsigned char>(lower[k]));
  }

  size_t i = 0;
  size_t htmlStart = 0;
  size_t counted = 0; // newline count for lua
  size_t lines = 0;   // newlines for contents

  while (i < n) {
    if (contents[i] != '<') {
      i += 1;
      continue;
    }

    // HTML comment
    if (lower.compare(i, 4, "<!--") == 0) {
      size_t end = contents.find("-->", i + 4);

      if (end == std::string::npos) {
        i = n;
      } else {
        i = end + 3;
      }

      continue;
    }

    // CDATA
    if (lower.compare(i, 9, "<![cdata[") == 0) {
      size_t end = contents.find("]]>", i + 9);

      if (end == std::string::npos) {
        i = n;
      } else {
        i = end + 3;
      }

      continue;
    }

    // Doctype / other
    if (i + 1 < n && contents[i + 1] == '!') {
      size_t end = contents.find('>', i + 2);

      if (end == std::string::npos) {
        i = n;
      } else {
        i = end + 1;
      }

      continue;
    }

    // Processing instruction
    if (i + 1 < n && contents[i + 1] == '?') {
      size_t end = contents.find("?>", i + 2);

      if (end == std::string::npos) {
        i = n;
      } else {
        i = end + 2;
      }

      continue;
    }

    // Anything else that isn't an opening tag is plain text
    if (i + 1 >= n ||
        !std::isalpha(static_cast<unsigned char>(contents[i + 1]))) {
      i += 1;
      continue;
    }

    // Read oppening tag name
    size_t nameEnd = i + 1;
    while (nameEnd < n &&
           (std::isalnum(static_cast<unsigned char>(contents[nameEnd])) ||
            contents[nameEnd] == '-' || contents[nameEnd] == ':' ||
            contents[nameEnd] == '_')) {
      nameEnd += 1;
    }
    std::string name = lower.substr(i + 1, nameEnd - i - 1);

    // Find the end of the tag
    size_t j = nameEnd;
    char prev = 0;
    while (j < n && contents[j] != '>') {
      char c = contents[j];

      if ((c == '"' || c == '\'') && prev == '=') {
        size_t end = contents.find(c, j + 1);

        if (end == std::string::npos) {
          j += 1;
        } else {
          j = end + 1;
        }

        prev = c;
        continue;
      }

      if (!std::isspace(static_cast<unsigned char>(c))) {
        prev = c;
      }

      j += 1;
    }

    if (j >= n) {
      i = nameEnd;
      continue;
    }

    bool selfClosing = contents[j - 1] == '/';
    size_t tagStart = i;
    size_t tagEnd = j + 1;

    // treat script/style as raw
    if ((name == "script" || name == "style") && !selfClosing) {
      size_t end = lower.find("</" + name, tagEnd);
      if (end == std::string::npos) {
        i = n;
      } else {
        i = end;
      }
      continue;
    }

    // Any other tag
    if (name != "lua" || selfClosing) {
      i = tagEnd;
      continue;
    }

    // Lua
    if (tagStart > htmlStart) {
      parsedContent.push_back(
          {false, contents.substr(htmlStart, tagStart - htmlStart)});
    }

    size_t luaStart = tagEnd;
    size_t luaEnd = n; // Treat rest of lua if unclosed
    size_t next = n;
    size_t p = luaStart;

    while (p < n) {
      char c = contents[p];

      // Closing lua tag
      if (c == '<' && lower.compare(p, 5, "</lua") == 0 && p + 5 < n &&
          (contents[p + 5] == '>' ||
           std::isspace(static_cast<unsigned char>(contents[p + 5])))) {
        size_t gt = contents.find('>', p + 5);
        luaEnd = p;

        if (gt == std::string::npos) {
          next = n;
        } else {
          next = gt + 1;
        }

        break;
      }

      // Lua comments
      bool isComment = (c == '-' && p + 1 < n && contents[p + 1] == '-');
      if (isComment || c == '[') {
        size_t b = p;

        if (isComment) {
          b = p + 2;
        }

        bool isLong = false;

        if (b < n && contents[b] == '[') {
          size_t m = b + 1;

          while (m < n && contents[m] == '=') {
            m += 1;
          }

          if (m < n && contents[m] == '[') {
            isLong = true;
            std::string closer = "]" + std::string(m - b - 1, '=') + "]";
            size_t end = contents.find(closer, m + 1);

            if (end == std::string::npos) {
              p = n;
            } else {
              p = end + closer.size();
            }
          }
        }

        if (!isLong) {
          if (isComment) {
            p = b;

            while (p < n && contents[p] != '\n') {
              p += 1;
            }
          } else {
            p += 1;
          }
        }

        continue;
      }

      // Strings
      if (c == '"' || c == '\'') {
        p += 1;

        while (p < n && contents[p] != c && contents[p] != '\n') {
          if (contents[p] == '\\' && p + 1 < n) {
            p += 1;
          }

          p += 1;
        }

        if (p < n && contents[p] == c) {
          p += 1;
        }

        continue;
      }

      p += 1;
    }

    // Pad the code to match line number
    lines += std::count(contents.begin() + counted, contents.begin() + luaStart,
                        '\n');
    counted = luaStart;

    if (luaEnd > luaStart) {
      parsedContent.push_back(
          {true, std::string(lines, '\n') +
                     contents.substr(luaStart, luaEnd - luaStart)});
    }

    i = next;
    htmlStart = next;
  }

  if (htmlStart < n) {
    parsedContent.push_back({false, contents.substr(htmlStart)});
  }
}

// Read the file and feed it to parser
std::vector<Parser::parseItem> Parser::readFile(std::string filepath) {
  // Load file contents
  std::ifstream file(filepath);

  std::string contents((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());

  file.close();

  std::vector<Parser::parseItem> parsedContent;

  parseContents(contents, parsedContent);

  return parsedContent;
}
} // namespace html::prs