#include <fstream>
#include <iostream>
#include <pugixml.hpp>
#include <string>

#include "./parser.h"

namespace html::prs {

// Escape CDATA
std::string Parser::escapeCData(std::string input) {
  std::string out;
  size_t pos = 0;

  while (true) {
    size_t found = input.find("]]>", pos);

    if (found == std::string::npos) {
      out += input.substr(pos);
      break;
    }

    out += input.substr(pos, found - pos);
    out += "]]]]><![CDATA[>";
    pos = found + 3;
  }

  return out;
}

// Process tag which content's should be wrapper in the CDATA
std::string Parser::preprocessCDataTag(std::string xml, std::string tagName) {
  std::string result;
  size_t pos = 0;

  std::string openPrefix = "<" + tagName;
  std::string closeTag = "</" + tagName + ">";

  while (true) {
    size_t start = xml.find(openPrefix, pos);

    if (start == std::string::npos) {
      result += xml.substr(pos);
      break;
    }

    size_t afterName = start + openPrefix.size();

    if (afterName < xml.size()) {
      char c = xml[afterName];
      if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_') {
        result += xml.substr(pos, afterName - pos);
        pos = afterName;
        continue;
      }
    }

    size_t tagClose = xml.find('>', afterName);

    if (tagClose == std::string::npos) {

      result += xml.substr(pos);
      break;
    }

    result += xml.substr(pos, start - pos);

    std::string openTag = xml.substr(start, tagClose - start + 1);
    bool selfClosing = tagClose > 0 && xml[tagClose - 1] == '/';

    result += openTag;

    if (selfClosing) {
      pos = tagClose + 1;
      continue;
    }

    size_t contentStart = tagClose + 1;
    size_t end = xml.find(closeTag, contentStart);

    if (end == std::string::npos) {
      result += xml.substr(contentStart);
      break;
    }

    std::string content = xml.substr(contentStart, end - contentStart);

    result += "<![CDATA[";
    result += escapeCData(content);
    result += "]]>";
    result += closeTag;

    pos = end + closeTag.size();
  }

  return result;
}

// Escape needed tags
std::string Parser::preprocessTags(std::string xml) {
  // Escape tags with code
  std::string result = preprocessCDataTag(xml, "lua");
  result = preprocessCDataTag(result, "style");
  result = preprocessCDataTag(result, "script");

  return result;
}

/// @brief Read the file and feed it to pugixml to generate the tree
/// @param filepath file's filepath to read
/// @return pugi::xml_document
pugi::xml_document Parser::readFile(std::string filepath) {
  // Load file contents
  std::ifstream file(filepath);

  std::string contents((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());

  // Preprocess them
  contents = preprocessTags(contents);

  // Feed to pugixml
  pugi::xml_document doc;

  if (!doc.load_string(contents.c_str(),
                       pugi::parse_default | pugi::parse_comments))
    return doc; // TODO: Add error handeling

  return doc;
}
} // namespace html::prs