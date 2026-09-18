#include "./creator.h"
#include "../../lua/manager/manager.h"
#include <algorithm>
#include <map>
#include <pugixml.hpp>
#include <sstream>
#include <string>

namespace html::crt {

// Escape data for html
std::string Creator::escape(std::string s) {
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

// Render the html
void Creator::render(pugi::xml_node node, std::ostringstream &out,
                     lua::mngr::LuaManager &script, std::string filename) {
  if (error) { // Exit if got an error
    return;
  }

  switch (node.type()) {
  case pugi::node_pcdata: // Normal text
    out << escape(node.value());
    break;
  case pugi::node_element: { // HTML element
    std::string tag = node.name();

    // Execute lua
    if (tag == "lua") {
      std::string code = node.child_value();

      // Add newlines based on tag position, which we got from parser, for error
      // handeling inside lua
      int luaStart = node.attribute("lua-start").as_int(0);
      if (luaStart > 0) {
        code = std::string(luaStart, '\n') + code;
      }

      auto [success, result] = script.runCode(code, filename);

      // Lua code failed
      if (!success) {
        result = "<p><b>[LUA ERROR!]</b><br/>" + result + "</p>";
      }

      // If result is not empty, parse and render its content
      if (result != "") {
        pugi::xml_document frag;
        frag.load_string(("<r>" + result + "</r>").c_str());
        for (auto child : frag.child("r").children())
          render(child, out, script, filename);
      }

      // Set error stopper value on error
      if (!success) {
        error = true;
      }

    } else { // Create the tag
      out << '<' << tag;

      // Populate its content
      for (auto attr : node.attributes())
        out << ' ' << attr.name() << "=\"" << escape(attr.value()) << '"';
      if (node.children().empty()) {
        out << "/>";
      } else {
        out << '>';
        for (auto child : node.children())
          render(child, out, script, filename);
        out << "</" << tag << '>';
      }
    }
    break;
  }
  case pugi::node_cdata: // Add data in script/style tags
    out << node.value();
    break;
  case pugi::node_comment: // Add comments
    out << "<!--" << node.value() << "-->";
    break;
  default:
    break;
  }
}

std::string
Creator::createHTML(const pugi::xml_document &doc, std::string fp,
                    std::map<std::string, std::string> cgi,
                    std::map<std::string, std::string> headers,
                    std::string body,
                    std::map<std::string, std::string> &httpHeaders) {
  std::ostringstream out;
  lua::mngr::LuaManager script(fp, cgi, headers, body, httpHeaders);

  error = false;

  // Get contents from sm-wrap-content wrapper
  pugi::xml_node root = doc.document_element();

  out << (root.attribute("add-doctype").as_bool() ? "<!DOCTYPE html>" : "");

  // Build for each child in the document
  for (auto child : root.children())
    render(child, out, script, cgi["SCRIPT_NAME"]);

  return out.str();
}
} // namespace html::crt