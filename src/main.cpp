#include <filesystem>
#include <iostream>
#include <pugixml.hpp>
#include <string>

#include "./html/creator/creator.h"
#include "./html/parser/parser.h"

int main() {
  std::string fp =
      std::filesystem::absolute(
          std::filesystem::path("../testbed/test.sm").parent_path())
          .string();
  
  html::prs::Parser parser;
  pugi::xml_document content = parser.readFile("../testbed/test.sm");
  html::crt::Creator creator;
  std::string res = creator.createHTML(content, fp);

  std::cout << res << std::endl;

  return 0;
}