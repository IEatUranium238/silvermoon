#ifndef PARSER_H
#define PARSER_H
#pragma once
#include <string>

namespace html::prs {
class Parser {
public:
  struct parseItem {
    bool isLua;
    std::string content;
  };

  std::vector<parseItem>  readFile(std::string filepath);
};
} // namespace html::prs
#endif
