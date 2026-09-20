#include "./html/creator/creator.h"
#include "./html/parser/parser.h"
#include "./lua/api/api.h"
#include <fcgiapp.h>
#include <filesystem>
#include <iostream>
#include <map>
#include <string_view>
#include <thread>

// UNIX-like specific stuff for unix socks
#ifndef _WIN32
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

const char *sockPath = "/var/run/silvermoon_fcgi.sock";
#endif

void worker(FCGX_Request *request) {
  std::map<std::string, std::string> cgi;
  std::map<std::string, std::string> headers;
  std::map<std::string, std::variant<std::string, double, bool>> cookies;
  std::map<std::string, std::string> params;
  std::string body;

  // CGI variables
  for (char **env = request->envp; *env != nullptr; env++) {
    std::string entry(*env);

    size_t split = entry.find('=');

    if (split == std::string::npos)
      continue;

    std::string key = entry.substr(0, split);
    std::string value = entry.substr(split + 1);

    // HTTP headers start with HTTP_
    if (key.rfind("HTTP_", 0) == 0) {
      std::string headerName = key.substr(5);
      headers[headerName] = value;
      continue;
    }

    cgi[key] = value;
  }

  // Parse cookies
  if (auto it = headers.find("COOKIE"); it != headers.end()) {
    std::string_view cookieHeader = it->second;

    while (!cookieHeader.empty()) {
      // Skip whitespace
      cookieHeader.remove_prefix(std::min(
          cookieHeader.find_first_not_of(" ;\t"), cookieHeader.size()));

      if (cookieHeader.empty()) {
        break;
      }

      size_t end = cookieHeader.find(';');
      std::string_view cookie = cookieHeader.substr(0, end);

      size_t equals = cookie.find('=');
      if (equals != std::string_view::npos) {
        std::string name(cookie.substr(0, equals));
        std::string value(cookie.substr(equals + 1));

        // Trim whitespace
        size_t nameStart = name.find_first_not_of(" \t");
        size_t nameEnd = name.find_last_not_of(" \t");

        if (nameStart != std::string::npos) {
          name = name.substr(nameStart, nameEnd - nameStart + 1);
        }

        cookies[name] = value;
      }

      if (end == std::string_view::npos) {
        break;
      }

      cookieHeader.remove_prefix(end + 1);
    }
  }

  lua::api::APIs urlApi;

  // Parse URL parameters
  if (auto it = cgi.find("REQUEST_URI"); it != headers.end()) {
    std::string_view url = it->second;

    size_t queryStart = url.find('?');
    if (queryStart != std::string_view::npos) {
      std::string_view query = url.substr(queryStart + 1);

      // Ignore fragment
      size_t fragmentStart = query.find('#');
      if (fragmentStart != std::string_view::npos) {
        query = query.substr(0, fragmentStart);
      }

      while (!query.empty()) {
        size_t end = query.find('&');
        std::string_view param = query.substr(0, end);

        size_t equals = param.find('=');

        std::string name;
        std::string value;

        if (equals != std::string_view::npos) {
          name = std::string(param.substr(0, equals));
          value = std::string(param.substr(equals + 1));
        } else {
          // Parameters without = treated as empty
          name = std::string(param);
        }

        // URL-decode
        name = urlApi.unescapeURL(name);
        value = urlApi.unescapeURL(value);

        params[name] = value;

        if (end == std::string_view::npos) {
          break;
        }

        query.remove_prefix(end + 1);
      }
    }
  }

  // Read body
  char buffer[4096];

  int read;
  while ((read = FCGX_GetStr(buffer, sizeof(buffer), request->in)) > 0) {
    body += std::string(buffer, read);
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
    std::string res = "Status: 404\r\nContent-Type: text/html\r\n";
    res += "\r\n";
    FCGX_FPrintF(request->out, "%s", res.c_str());

    FCGX_Finish_r(request);
    delete request;
    return;
  }

  // Do stuff

  html::prs::Parser parser;
  pugi::xml_document parsedDoc = parser.readFile(script);

  std::map<std::string, std::string> resHeaders;
  html::crt::Creator creator;

  resHeaders["Status"] = "200";
  resHeaders["Content-Type"] = "text/html";

  std::string res = creator.createHTML(parsedDoc, script, cgi, headers, body,
                                       resHeaders, cookies, params);

  std::string headerString = "";

  // Create header string

  for (auto &[k, v] : resHeaders) {
    headerString += k + ": " + v + "\r\n";
  }

  // Give the result
  res = headerString + "\r\n" + res;

  FCGX_FPrintF(request->out, "%s", res.c_str());

  FCGX_Finish_r(request);
  delete request;
}

int main() {
  bool usingUnixSockets = false;

  // Unix socket setting discovery
  const char *usEnv = std::getenv("SM_USE_UNIXSOCKS");
  if (usEnv != nullptr) {
    if (usEnv == "true") {
#ifdef _WIN32
      std::cerr << "UNIX sockets are only usable on UNIX-like OSes!"
                << std::endl;
      return 1;
#else
      usingUnixSockets = true;

      // Create the socket server
      int server = socket(AF_UNIX, SOCK_STREAM, 0);
      if (server == -1) {
        std::cerr << "UNIX socket creation failed" << std::endl;
        return 1;
      }

      unlink(sockPath);

      // Some cursed POSIX api stuff
      struct sockaddr_un addr;
      std::memset(&addr, 0, sizeof(addr));
      addr.sun_family = AF_UNIX;
      std::strncpy(addr.sun_path, sockPath, sizeof(addr.sun_path) - 1);

      // Bind the socket
      if (bind(server, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        std::cerr << "UNIX socket bind failed" << std::endl;
        close(server);
        return 1;
      }
#endif
    }
  }

  if (FCGX_Init() != 0) {
    std::cerr << "Failed to initialize FastCGI" << std::endl;
    return 1;
  }

  int socket = FCGX_OpenSocket(usingUnixSockets ? sockPath : ":9000", 100);

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

#ifndef _WIN32
  // Remove the unix socket if used
  if (usingUnixSockets) {
    unlink(sockPath);
  }
#endif

  return 0;
}