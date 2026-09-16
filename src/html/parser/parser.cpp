#include <algorithm>
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

    // Add special internal atribute for lua tags for padding in the future
    if (tagName == "lua") {
      size_t lineNum = std::count(xml.begin(), xml.begin() + start, '\n');
      std::string attr = " lua-start=\"" + std::to_string(lineNum) + "\"";

      size_t insertOffset =
          selfClosing ? openTag.size() - 2 : openTag.size() - 1;
      openTag.insert(insertOffset, attr);
    }

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

  // Check whether DOCTYPE exists at the start of the document for atribute
  bool hasDoctype = false;

  size_t first = contents.find_first_not_of(" \t\r\n");

  if (first != std::string::npos &&
      (contents.compare(first, 9, "<!DOCTYPE") == 0 ||
       contents.compare(first, 9, "<!doctype") == 0)) {
    hasDoctype = true;

    size_t doctypeEnd = contents.find('>', first);

    if (doctypeEnd != std::string::npos) {
      contents.erase(first, doctypeEnd - first + 1);
    }
  }

  // Wrap the contents so they are valid
  contents = "<sm-wrap-content add-doctype=\"" +
             std::string(hasDoctype ? "true" : "false") + "\">" + contents +
             "</sm-wrap-content>";

  // Feed to pugixml
  pugi::xml_document doc;

  pugi::xml_parse_result result = doc.load_string(
      contents.c_str(),
      pugi::parse_default | pugi::parse_comments | pugi::parse_doctype);

  if (!result) {
    pugi::xml_document errDoc;

    // Create the HTML
    std::string errorHtml =
        "<html>"
        "<head>"
        "<title>Silvermoon Parsing Error</title>"
        "<style>"
        "body {"
        "    font-family: sans-serif;"
        "    max-width: 800px;"
        "    margin: 40px auto;"
        "    padding: 0 20px;"
        "    color: #333;"
        "    background: #f5f5f5;"
        "}"
        "h1 {"
        "    color: #b42318;"
        "}"
        "h2 {"
        "    margin-top: 30px;"
        "}"
        "h3 {"
        "    margin-bottom: 5px;"
        "}"
        "p {"
        "    line-height: 1.5;"
        "}"
        "hr {"
        "    margin: 30px 0;"
        "    border: 0;"
        "    border-top: 1px solid #ccc;"
        "}"
        "small {"
        "    color: #777;"
        "}"
        "</style>"
        "</head>"
        "<body>"

        "<h1>Failed to parse Silvermoon XHTML5 document</h1>"

        "<p>Silvermoon encountered an error while trying to parse this "
        "Silvermoon XHTML5 document.</p>"

        "<h2>Error Details</h2>"
        "<p><strong>Description:</strong> " +
        std::string(result.description()) +
        "</p>"
        "<p><strong>Offset:</strong> " +
        std::to_string(result.offset) +
        "</p>"
        "<p><strong>File:</strong> " +
        filepath +
        "</p>"

        "<hr/>"

        "<h2>What to Do</h2>"

        "<h3>Site owner / Developer</h3>"
        "<p>"
        "Check the HTML document around the reported offset and look for "
        "invalid, malformed, or unsupported markup. Correct the document "
        "and try loading it again."
        "</p>"

        "<h3>Visitor</h3>"
        "<p>"
        "Try refreshing the page or returning to the previous page. "
        "If the problem persists, contact the site owner and report "
        "the error shown above."
        "</p>"

        "<hr/>"

        "<p><small>"
        "Silvermoon version " +
        SM_VERSION +
        "</small></p>"

        "</body>"
        "</html>";

    errDoc.load_string(errorHtml.c_str(), pugi::parse_default |
                                              pugi::parse_comments |
                                              pugi::parse_doctype);

    return errDoc;
  }
  return doc;
}
} // namespace html::prs