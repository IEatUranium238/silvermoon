# Silvermoon

Silvermoon is a XHTML5 preprocessor with power of Lua 5.1 via LuaJIT.

It allows you to embed lua via `<lua>` tags into your markup.

> NOTE: Silvermoon is in active development, updates might change APIs and other parts.

## Installation

Currently I only distribute compiled binaries for Linux (64 bit), as its what I am testing this on

### To compile the project you need following:

**Tools:**

- CMake
- C++ 17 or later

**Libraries:**

- pugixml
- luajit
- libfcgi

**After installing needed tools and libraries run following:**

```
cmake -S . -B build
```

and after that:

```
cmake --build build
```

**silvermoon binary should appear in build folder if everything goes fine**

## Configuration

You need to configure a web server of your choosing to use .sm files and redirect them to fastcgi

**Example configuration to add in Apache (000-default.conf):**

```
DirectoryIndex index.sm

ProxyPassMatch "^/(.*\.sm)$" "fcgi://127.0.0.1:9000/var/www/html/$1"
```

### UNIX Socket usage

You can also configure it to use an unix socket to communicate with the web server.
To do it, do following:

1. Set SM_USE_UNIXSOCKS env variable to "true" (as string, not a boolean)
2. Change your server config to use UNIX socket instead, example for Apache:

```
DirectoryIndex index.sm

ProxyPassMatch "^/(.*\.sm)$" "unix:/var/run/silvermoon_fcgi.sock|/var/www/html/$1"
```

3. Restart your web server and silvermoon

> NOTE: replace `/var/www/html/` with your Apache's web root if different!

## Writing code

Silvermoon usses XHTML5 (HTML 5 but following XML parsing rules) with .sm file extension as it's markup language file to embed lua into.

Example hello world program:

```html
<!DOCTYPE html>
<html lang="en">
  <head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <title>Hello, world!</title>
  </head>
  <body>
    <h1>Hello, <lua>return "world!"</lua></h1>
  </body>
</html>
```

You can use both return and print() to echo html content.

Silvermoon uses Lua 5.1.

**Following libraries are available for use:**

- base
- coroutine
- string
- os
- math
- table
- io
- utf8
- bit32

**Following functions are removed for safety reasons:**

- os.execute
- os.exit
- os.remove
- os.rename

- io.open
- io.popen

- dofile
- loadfile

**You can use external .lua files and luarocks packages via include**

## Running

After configuring your web server, to run do following:

1. Run the silvermoon binary. It should display "FastCGI server started on port 9000" if everything is running as intended.

2. Start your web server, or reload it if you changed configuration without shutdown.

3. Go to your file's url

If everything works correctly with code from example, you should see h1 tag with "Hello, world!"

## Current Silvermoon's APIs

Currently we only provide request data such as:

- sm.request - dictionary for general purpose info such as `REQUEST_METHOD`
- sm.header - dictionary for http headers (FastCGI headers that start with `HTTP_`, with that part removed from the key itself)
- sm.body - string for body content

**Key names use SCREAMING_SNAKE_CASE and match names given from FastCGI**

## Contributions

Contributions are welcome, create the PR and I will review it.

I currently don't enforce any coding style but it would be nice if you set your code formatter to LLVM style.

## Roadmap
- Add safer alternatives for removed functions in sm's API
- Create API that would allow some form of production use
- Other platform builds
- ???
