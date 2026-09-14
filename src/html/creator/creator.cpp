#include "./creator.h"
#include "../../lua/manager/manager.h"
#include <algorithm>
#include <pugixml.hpp>
#include <sstream>
#include <string>

extern "C" {
#include <tidy/tidy.h>
#include <tidy/tidybuffio.h>
}

namespace html::crt {

// Escape data for html
std::string Creator::escape(std::string s) {
  std::string out;

  for (char c : s) {
    if (c == '&')
      out += "&amp;";
    else if (c == '<')
      out += "&lt;";
    else if (c == '>')
      out += "&gt;";
    else
      out += c;
  }

  return out;
}

// Render the html
void Creator::render(pugi::xml_node node, std::ostringstream &out,
                     lua::mngr::LuaManager &script) {
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
      auto [success, result] = script.runCode(node.child_value());

      // Lua code failed
      if (!success) {
        result = "<p><b>[LUA ERROR!]</b><br/>" + result + "</p>";
      }

      // If result is not empty, parse and render its content
      if (result != "") {
        pugi::xml_document frag;
        frag.load_string(("<r>" + result + "</r>").c_str());
        for (auto child : frag.child("r").children())
          render(child, out, script);
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
          render(child, out, script);
        out << "</" << tag << '>';
      }
    }
    break;
  }
  case pugi::node_cdata: // Add data in script/style tags
    out << node.value();
    break;
  case pugi::node_comment: // Add comments
    out << "<!--" << escape(node.value()) << "-->";
    break;
  default:
    break;
  }
}

// Format the HTML
std::string Creator::beautifyHtml(const std::string &inputHtml) {
  TidyDoc tdoc = tidyCreate();

  // Some scary C library tomfoolery
  TidyBuffer output;
  TidyBuffer errbuf;
  tidyBufInit(&output);
  tidyBufInit(&errbuf);

  int rc = -1;

  // Configuration
  tidyOptSetInt(tdoc, TidyIndentContent, TidyIndentSpaces);
  tidyOptSetInt(tdoc, TidyIndentSpaces, 4);
  tidyOptSetInt(tdoc, TidyWrapLen, 0);
  tidyOptSetBool(tdoc, TidyMark, no);
  tidyOptParseValue(tdoc, "show-body-only", "auto");

  // Capture errors and warnings
  rc = tidySetErrorBuffer(tdoc, &errbuf);

  // Parse the input
  if (rc >= 0)
    rc = tidyParseString(tdoc, inputHtml.c_str());
  // Clean it
  if (rc >= 0)
    rc = tidyCleanAndRepair(tdoc);
  // Serialize back
  if (rc >= 0)
    rc = tidySaveBuffer(tdoc, &output);

  std::string resultHtml;

  if (rc >= 0) {
    resultHtml = reinterpret_cast<char *>(output.bp);
  } else {
    std::cerr << "Tidy Error: " << errbuf.bp << std::endl;
  }

  // Clean up memory
  tidyBufFree(&output);
  tidyBufFree(&errbuf);
  tidyRelease(tdoc);

  return resultHtml;
}

std::string Creator::createHTML(const pugi::xml_document &doc, std::string fp) {
  std::ostringstream out;
  lua::mngr::LuaManager script(fp);

  error = false;
  out << "<!DOCTYPE html>"; // Append doctype declaration

  // Get contents from sm-wrap-content wrapper
  pugi::xml_node root = doc.document_element();

  // Build for each child in the document
  for (auto child : root.children())
    render(child, out, script);

  // Format html and clean it up
  return beautifyHtml(out.str());
}
} // namespace html::crt