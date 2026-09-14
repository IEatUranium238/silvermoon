#include "./html/creator/creator.h"
#include "./html/parser/parser.h"
#include <fcgiapp.h>
#include <filesystem>
#include <iostream>
#include <map>
#include <thread>

void worker(FCGX_Request *request) {
  std::map<std::string, std::string> cgi;
  std::map<std::string, std::string> headers;
  std::map<std::string, std::string> body;

  // CGI variables
  for (char **env = request->envp; *env != nullptr; env++) {
    std::string entry(*env);

    size_t split = entry.find('=');

    if (split == std::string::npos)
      continue;

    std::string key = entry.substr(0, split);
    std::string value = entry.substr(split + 1);

    cgi[key] = value;

    // HTTP headers start with HTTP_
    if (key.rfind("HTTP_", 0) == 0) {
      std::string headerName = key.substr(5);
      headers[headerName] = value;
    }
  }

  // Read body
  char buffer[4096];

  int read;
  while ((read = FCGX_GetStr(buffer, sizeof(buffer), request->in)) > 0) {
    body["raw"] += std::string(buffer, read);
  }

  // Find the script
  std::string script = FCGX_GetParam("SCRIPT_FILENAME", request->envp);

  // Remove proxy trash if exists
  if (script.rfind("proxy:fcgi://127.0.0.1:9000", 0) == 0) {
    script.erase(0, std::string("proxy:fcgi://127.0.0.1:9000").size());
  }

  // Check if path exists

  std::filesystem::path checkpath = script;

  if (!std::filesystem::exists(checkpath)) {
    std::string res = "Status: 404\r\nContent-Type: text/html\r\n\r\n";
    FCGX_FPrintF(request->out, "%s", res.c_str());

    FCGX_Finish_r(request);
    delete request;
    return;
  }

  // Do stuff

  html::prs::Parser parser;
  pugi::xml_document parsedDoc = parser.readFile(script);
  html::crt::Creator creator;
  std::string res = creator.createHTML(parsedDoc, script);

  // Give the result
  res = "Status: 200\r\nContent-Type: text/html\r\n\r\n" + res;

  FCGX_FPrintF(request->out, "%s", res.c_str());

  FCGX_Finish_r(request);
  delete request;
}

int main() {
  if (FCGX_Init() != 0) {
    std::cerr << "Failed to initialize FastCGI" << std::endl;
    return 1;
  }

  int socket = FCGX_OpenSocket(":9000", 100);

  if (socket < 0) {
    std::cerr << "Failed to open FastCGI socket" << std::endl;
    return 1;
  }

  std::cout << "FastCGI server started on port 9000" << std::endl;

  while (true) {
    auto *request = new FCGX_Request;

    FCGX_InitRequest(request, socket, 0);

    int acceptRC = FCGX_Accept_r(request);

    if (acceptRC < 0) {
      std::cerr << "FCGX_Accept_r() failed" << std::endl;
      delete request;
      break;
    }

    std::thread(worker, request).detach();
  }

  return 0;
}