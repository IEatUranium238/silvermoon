#include "./html/creator/creator.h"
#include "./html/parser/parser.h"
#include "./lua/api/api.h"
#include "./lua/transport/transport.h"
#include <fcgiapp.h>
#include <filesystem>
#include <iostream>
#include <map>
#include <string_view>
#include <thread>

// UNIX-like specific stuff for unix socks
#ifndef _WIN32
#include <unistd.h>

const char *sockPath = "/var/run/silvermoon_fcgi.sock";
#endif

bool enableOpen = false;
bool enableExecute = false;
bool enableCL = false;
bool enableAdvFs = false;

lua::trpt::Transport transport;

void worker(FCGX_Request *request) {
  std::map<std::string, std::string> cgi;
  std::map<std::string, std::string> headers;
  std::map<std::string, std::variant<std::string, double, bool>> cookies;
  std::map<std::string, std::string> params;
  std::map<std::string, std::string> formdata;
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
    if (key.starts_with("HTTP_")) {
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
  if (auto it = cgi.find("QUERY_STRING"); it != cgi.end()) {
    std::string_view query = it->second;
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
        // Parameters without = are treated as empty
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

  // Read body
  char buffer[4096];

  int read;
  while ((read = FCGX_GetStr(buffer, sizeof(buffer), request->in)) > 0) {
    body += std::string(buffer, read);
  }


  std::string type = cgi["CONTENT_TYPE"];
  // Parse form body
  if (type == "application/x-www-form-urlencoded") {
    std::string_view query = body;
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
        // Parameters without = are treated as empty
        name = std::string(param);
      }

      // URL-decode
      name = urlApi.unescapeURL(name);
      value = urlApi.unescapeURL(value);

      formdata[name] = value;

      if (end == std::string_view::npos) {
        break;
      }

      query.remove_prefix(end + 1);
    }
  }

  if (type.starts_with("multipart/form-data")) {
    formdata = urlApi.parseMultipart(body, urlApi.getBoundary(type));
  }

  // Find the script
  std::string script = FCGX_GetParam("SCRIPT_FILENAME", request->envp);

  // Remove proxy trash if exists
  if (script.starts_with("proxy:fcgi://127.0.0.1:9000")) {
    script.erase(0, std::string("proxy:fcgi://127.0.0.1:9000").size());
  }

  if (script.starts_with("proxy:fcgi://localhost")) {
    script.erase(0, std::string("proxy:fcgi://localhost").size());
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
  std::string res;

  try {
    res = creator.createHTML(parsedDoc, script, cgi, headers, body, resHeaders,
                             cookies, params, enableOpen, enableAdvFs,
                             enableExecute, enableCL, transport, formdata);
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;

    resHeaders["Status"] = "500";
    res =
        "<html>"
        "<head>"
        "<title>Silvermoon Error</title>"
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

        "<h1>Internal Silvermoon error</h1>"

        "<p>Silvermoon encountered an error while trying to preprocess this "
        "HTML document.</p>"

        "<hr/>"

        "<h2>What to Do</h2>"

        "<h3>Site owner / Developer</h3>"
        "<p>"
        "Check Silvermoon and see if it produced any errors. If you are sure "
        "this is a bug on Silvermoon's side, please open issue on our <a "
        "href=\"https://github.com/IEatUranium238/silvermoon/\">github page</a>"
        "</p>"

        "<h3>Visitor</h3>"
        "<p>"
        "Try refreshing the page or returning to the previous page. "
        "If the problem persists, contact the site owner."
        "</p>"

        "<hr/>"

        "<p><small>"
        "Silvermoon version " SM_VERSION "</small></p>"

        "</body>"
        "</html>";
  }

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
  if (usEnv != nullptr && std::strcmp(usEnv, "true") == 0) {
#ifdef _WIN32
    std::cerr << "UNIX sockets are only usable on UNIX-like OSes!" << std::endl;
    return 1;
#else
    usingUnixSockets = true;
    unlink(sockPath); // Clear stale socket
#endif
  }

  // Safety settings
  const char *clEnv = std::getenv("SM_ENABLE_RISKY_CODELOADING");
  const char *executionEnv = std::getenv("SM_ENABLE_VERY_RISKY_SHELL");
  const char *openEnv = std::getenv("SM_ENABLE_RISKY_OPEN");
  const char *fsEnv = std::getenv("SM_ENABLE_VERY_RISKY_ADVANCED_FS");

  if (clEnv != nullptr && std::strcmp(clEnv, "true") == 0) {
    enableCL = true;
  }

  if (executionEnv != nullptr && std::strcmp(executionEnv, "true") == 0) {
    enableExecute = true;
  }

  if (openEnv != nullptr && std::strcmp(openEnv, "true") == 0) {
    enableOpen = true;
  }

  if (fsEnv != nullptr && std::strcmp(fsEnv, "true") == 0) {
    enableAdvFs = true;
  }

  // FCGI init
  if (FCGX_Init() != 0) {
    std::cerr << "Failed to initialize FastCGI" << std::endl;
    return 1;
  }

  int socket = FCGX_OpenSocket(usingUnixSockets ? sockPath : ":9000", 100);

  if (socket < 0) {
    std::cerr << "Failed to open FastCGI socket" << std::endl;
    return 1;
  }

  std::cout << (usingUnixSockets ? "FastCGI server started at UNIX socket at "
                                   "/var/run/silvermoon_fcgi.sock"
                                 : "FastCGI server started on TCP port 9000")
            << std::endl;

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