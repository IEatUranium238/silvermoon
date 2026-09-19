#ifndef PARSER_H
#define PARSER_H
#pragma once
#include <pugixml.hpp>
#include <string>

namespace html::prs {
class Parser {
private:
  std::string preprocessTags(std::string xml);
  std::string preprocessCDataTag(std::string xml, std::string tagName);
  std::string escapeCData(std::string input);

public:
  pugi::xml_document readFile(std::string filepath);
};
} // namespace html::prs
#endif
