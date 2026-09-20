#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <pugixml.hpp>
#include <string>
#include <unordered_set>
#include <vector>

#include "./parser.h"

namespace html::prs {

// CAUTION: This shit is ugly as fuck
// Keep small children and pets away from it, IT WILL EAT THEM

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

// Preprocess HTML to be XML valid
std::string Parser::preprocessTags(std::string xml) {
  static const std::unordered_set<std::string> scTags = {
      "area",   "base",     "br",      "col",   "embed",  "hr",    "img",
      "input",  "link",     "meta",    "param", "source", "track", "wbr",
      "keygen", "basefont", "bgsound", "frame", "isindex"};

  static const std::unordered_set<std::string> closesP = {
      "address", "article", "aside",    "blockquote", "details", "dialog",
      "div",     "dl",      "fieldset", "figcaption", "figure",  "footer",
      "form",    "h1",      "h2",       "h3",         "h4",      "h5",
      "h6",      "header",  "hgroup",   "hr",         "main",    "menu",
      "nav",     "ol",      "p",        "pre",        "section", "table",
      "ul"};

  std::string result;
  result.reserve(xml.size());

  std::vector<std::string> stack;

  // Helpers
  auto lower = [](std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
      return static_cast<char>(std::tolower(c));
    });
    return s;
  };

  auto isNameChar = [](char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '-' ||
           c == '_' || c == ':';
  };

  size_t pos = 0;

  while (pos < xml.size()) {
    // Normal text
    if (xml[pos] != '<') {
      result += xml[pos++];
      continue;
    }

    // Comment
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

    // CData
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

    // Find end of tag
    size_t end = pos + 1;
    char quote = '\0';

    while (end < xml.size()) {
      char c = xml[end];

      if (quote != '\0') {
        if (c == quote)
          quote = '\0';
      } else {
        if (c == '"' || c == '\'')
          quote = c;
        else if (c == '>')
          break;
      }

      end += 1;
    }

    if (end >= xml.size()) {
      result += xml.substr(pos);
      break;
    }

    std::string tag = xml.substr(pos, end - pos + 1);

    if (tag.size() >= 2 && (tag[1] == '/' || tag[1] == '!' || tag[1] == '?')) {
      result += tag;

      // Closing tag
      if (tag[1] == '/') {
        size_t nameStart = 2;
        size_t nameEnd = nameStart;

        while (nameEnd < tag.size() && isNameChar(tag[nameEnd])) {
          nameEnd += 1;
        }

        std::string tagName = lower(tag.substr(nameStart, nameEnd - nameStart));

        for (auto it = stack.rbegin(); it != stack.rend(); it++) {
          if (*it == tagName) {
            stack.erase(std::next(it).base());
            break;
          }
        }
      }

      pos = end + 1;
      continue;
    }

    // Extract opening tag name

    size_t nameStart = pos + 1;
    size_t nameEnd = nameStart;

    while (nameEnd < end && isNameChar(xml[nameEnd])) {
      nameEnd += 1;
    }

    std::string tagName = lower(xml.substr(nameStart, nameEnd - nameStart));

    // Process attributes + boolean attributes

    std::string processed;
    processed.reserve(tag.size() + 16);

    // Copy '<' + tag name
    processed.append(tag, 0, nameEnd - pos);

    size_t i = nameEnd - pos;

    while (i < tag.size()) {
      // Whitespace
      if (std::isspace(static_cast<unsigned char>(tag[i]))) {
        processed += tag[i++];
        continue;
      }

      // End
      if (tag[i] == '>') {
        processed += '>';
        i += 1;
        break;
      }

      // Self-closing
      if (tag[i] == '/' && i + 1 < tag.size() && tag[i + 1] == '>') {

        processed += "/>";
        i += 2;
        break;
      }

      // Attribute name
      size_t attrStart = i;

      while (i < tag.size() &&
             !std::isspace(static_cast<unsigned char>(tag[i])) &&
             tag[i] != '=' && tag[i] != '>' && tag[i] != '/') {
        i += 1;
      }

      if (attrStart == i) {
        processed += tag[i++];
        continue;
      }

      std::string attrName = tag.substr(attrStart, i - attrStart);

      // Whitespace after attribute name
      size_t afterName = i;

      while (afterName < tag.size() &&
             std::isspace(static_cast<unsigned char>(tag[afterName]))) {
        afterName += 1;
      }

      // Boolean attribute
      if (afterName >= tag.size() || tag[afterName] != '=') {
        processed += attrName;
        processed += "=\"\"";
        i = afterName;
        continue;
      }

      // Normal attribute
      processed += attrName;
      i = afterName;
      processed += '=';

      i += 1;

      // Preserve whitespace after '='
      while (i < tag.size() &&
             std::isspace(static_cast<unsigned char>(tag[i]))) {
        processed += tag[i++];
      }

      if (i >= tag.size())
        break;

      // Quoted value
      if (tag[i] == '"' || tag[i] == '\'') {
        char q = tag[i];

        processed += tag[i++];

        while (i < tag.size()) {
          char c = tag[i++];
          processed += c;

          if (c == q)
            break;
        }

        continue;
      }

      // Unquoted value
      while (i < tag.size() &&
             !std::isspace(static_cast<unsigned char>(tag[i])) &&
             tag[i] != '>') {

        if (tag[i] == '/' && i + 1 < tag.size() && tag[i + 1] == '>') {
          break;
        }

        processed += tag[i++];
      }
    }

    tag = std::move(processed);

    // Self-closing HTML tags

    size_t check = tag.size() - 1;

    while (check > 0 &&
           std::isspace(static_cast<unsigned char>(tag[check - 1]))) {
      check -= 1;
    }

    bool selfClosing = check > 0 && tag[check - 1] == '/';

    if (!selfClosing && scTags.contains(tagName)) {
      tag.insert(tag.size() - 1, "/");
      selfClosing = true;
    }

    // Optional closing tags

    auto closeTopIf = [&](auto predicate) {
      if (!stack.empty() && predicate(stack.back())) {
        result += "</" + stack.back() + ">";
        stack.pop_back();
        return true;
      }
      return false;
    };

    if (tagName == "li") {
      closeTopIf([](const std::string &s) { return s == "li"; });
    }

    if (tagName == "dt" || tagName == "dd") {
      closeTopIf([](const std::string &s) { return s == "dt" || s == "dd"; });
    }

    if (tagName == "option") {
      closeTopIf([](const std::string &s) { return s == "option"; });
    }

    if (tagName == "optgroup") {
      closeTopIf([](const std::string &s) { return s == "option"; });
      closeTopIf([](const std::string &s) { return s == "optgroup"; });
    }

    if (tagName == "rt" || tagName == "rp") {
      closeTopIf([](const std::string &s) { return s == "rt" || s == "rp"; });
    }

    if (tagName == "tr") {
      closeTopIf([](const std::string &s) { return s == "tr"; });
    }

    if (tagName == "td" || tagName == "th") {
      closeTopIf([](const std::string &s) { return s == "td" || s == "th"; });
    }

    if (tagName == "thead" || tagName == "tbody" || tagName == "tfoot") {
      closeTopIf([](const std::string &s) {
        return s == "thead" || s == "tbody" || s == "tfoot";
      });
    }

    if (closesP.contains(tagName)) {
      closeTopIf([](const std::string &s) { return s == "p"; });
    }

    // Code tags
    bool isCDataTag =
        tagName == "lua" || tagName == "style" || tagName == "script";

    // lua-start attribute
    if (tagName == "lua") {
      size_t lineNum = std::count(xml.begin(), xml.begin() + pos, '\n');

      std::string attr = " lua-start=\"" + std::to_string(lineNum) + "\"";

      size_t insertOffset = tag.size() - 1;

      tag.insert(insertOffset, attr);
    }

    result += tag;

    // Ignore self closing tags
    if (selfClosing) {
      pos = end + 1;
      continue;
    }

    // Wrap cdata
    if (isCDataTag) {
      size_t contentStart = end + 1;
      std::string closeTag = "</" + tagName + ">";

      size_t close = xml.find(closeTag, contentStart);

      if (close == std::string::npos) {
        result += xml.substr(contentStart);
        break;
      }

      std::string content = xml.substr(contentStart, close - contentStart);

      result += "<![CDATA[";
      result += escapeCData(content);
      result += "]]>";
      result += closeTag;

      pos = close + closeTag.size();
      continue;
    }

    stack.push_back(tagName);
    pos = end + 1;
  }

  // Close remaining
  while (!stack.empty()) {
    result += "</" + stack.back() + ">";
    stack.pop_back();
  }

  return result;
}

/// Read the file and feed it to pugixml to generate the tree
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

        "<h1>Failed to parse Silvermoon HTML document</h1>"

        "<p>Silvermoon encountered an error while trying to parse this "
        "Silvermoon HTML document.</p>"

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