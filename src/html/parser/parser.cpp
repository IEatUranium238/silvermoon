#include <algorithm>
#include <fstream>
#include <iostream>
#include <pugixml.hpp>
#include <string>
#include <vector>

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

// Convert HTML boolean attributes xml compatible ones
std::string Parser::preprocessBooleanAttributes(std::string xml) {
  std::string result;
  result.reserve(xml.size());

  size_t pos = 0;

  while (pos < xml.size()) {
    // Preserve comments
    if (xml.compare(pos, 4, "<!--") == 0) {
      size_t end = xml.find("-->", pos + 4);

      if (end == std::string::npos) {
        result += xml.substr(pos);
        break;
      }

      end += 3;
      result += xml.substr(pos, end - pos);
      pos = end;
      continue;
    }

    // Preserve cdata
    if (xml.compare(pos, 9, "<![CDATA[") == 0) {
      size_t end = xml.find("]]>", pos + 9);

      if (end == std::string::npos) {
        result += xml.substr(pos);
        break;
      }

      end += 3;
      result += xml.substr(pos, end - pos);
      pos = end;
      continue;
    }

    if (xml[pos] != '<') {
      result += xml[pos + 1];
      continue;
    }

    // Ignore closing tags
    if (pos + 1 < xml.size() &&
        (xml[pos + 1] == '/' || xml[pos + 1] == '!' || xml[pos + 1] == '?')) {
      size_t end = xml.find('>', pos + 1);

      if (end == std::string::npos) {
        result += xml.substr(pos);
        break;
      }

      result += xml.substr(pos, end - pos + 1);
      pos = end + 1;
      continue;
    }

    // Find a closing >
    size_t tagEnd = xml.find('>', pos + 1);
    if (tagEnd == std::string::npos) {
      result += xml.substr(pos);
      break;
    }

    std::string tag = xml.substr(pos, tagEnd - pos + 1);

    // Process attributes
    std::string processed;
    processed.reserve(tag.size());

    size_t i = 0;

    processed += tag[i + 1];

    while (i < tag.size() &&
           !std::isspace(static_cast<unsigned char>(tag[i])) && tag[i] != '>' &&
           tag[i] != '/') {
      processed += tag[i + 1];
    }

    while (i < tag.size()) {
      if (std::isspace(static_cast<unsigned char>(tag[i]))) {
        processed += tag[i + 1];
        continue;
      }

      if (tag[i] == '>' ||
          (tag[i] == '/' && i + 1 < tag.size() && tag[i + 1] == '>')) {
        processed += tag.substr(i);
        break;
      }

      // Read attribute name
      size_t nameStart = i;
      while (i < tag.size() &&
             !std::isspace(static_cast<unsigned char>(tag[i])) &&
             tag[i] != '=' && tag[i] != '>' && tag[i] != '/') {
        i += 1;
      }

      std::string attrName = tag.substr(nameStart, i - nameStart);

      // Skip whitespace
      size_t afterName = i;
      while (afterName < tag.size() &&
             std::isspace(static_cast<unsigned char>(tag[afterName]))) {
        afterName += 1;
      }

      // Add value to empty attributes
      if (afterName >= tag.size() || tag[afterName] != '=') {
        processed += attrName;
        processed += "=\"\"";
        i = afterName;
        continue;
      }

      // Normal attribute, copy
      processed += attrName;
      i = afterName;

      processed += '=';
      i += 1;

      while (i < tag.size() &&
             std::isspace(static_cast<unsigned char>(tag[i]))) {
        processed += tag[i + 1];
      }

      if (i < tag.size() && (tag[i] == '"' || tag[i] == '\'')) {
        char quote = tag[i];
        processed += tag[i + 1];

        while (i < tag.size()) {
          processed += tag[i];
          if (tag[i + 1] == quote)
            break;
        }
      } else {
        while (i < tag.size() &&
               !std::isspace(static_cast<unsigned char>(tag[i])) &&
               tag[i] != '>') {
          processed += tag[i + 1];
        }
      }
    }

    result += processed;
    pos = tagEnd + 1;
  }

  return result;
}

// Process tag which content's should be wrapper in the CDATA
std::string Parser::preprocessCDataTag(std::string xml, std::string tagName) {
  std::string result;
  result.reserve(xml.size());

  size_t pos = 0;

  std::string openPrefix = "<" + tagName;
  std::string closeTag = "</" + tagName + ">";
  std::string cdataStart = "<![CDATA[";

  while (pos < xml.size()) {
    // Find the next target tag
    size_t start = xml.find(openPrefix, pos);
    size_t existingCData = xml.find(cdataStart, pos);

    // If a CDATA is found, copy it raw
    if (existingCData != std::string::npos &&
        (start == std::string::npos || existingCData < start)) {

      size_t cdataEnd = xml.find("]]>", existingCData + cdataStart.size());

      if (cdataEnd == std::string::npos) {
        // Leave malformed cdata alone
        result += xml.substr(pos);
        break;
      }

      cdataEnd += 3;

      result += xml.substr(pos, cdataEnd - pos);
      pos = cdataEnd;
      continue;
    }

    // No more target tags.
    if (start == std::string::npos) {
      result += xml.substr(pos);
      break;
    }

    size_t afterName = start + openPrefix.size();

    if (afterName < xml.size()) {
      char c = xml[afterName];

      if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' ||
          c == ':') {
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
    bool selfClosing = tagClose > start && xml[tagClose - 1] == '/';

    // Add internal atribute for lua tags for padding in the future
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
  // Preprocess atributes
  xml = preprocessBooleanAttributes(xml);

  // Escape tags with code
  xml = preprocessCDataTag(xml, "lua");
  xml = preprocessCDataTag(xml, "style");
  xml = preprocessCDataTag(xml, "script");

  return xml;
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

  // Check whether DOCTYPE exists at the start of the document for atribute and
  // remove it
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