cmake_minimum_required(VERSION 3.10.0)

project(silvermoon VERSION 0.4.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# MSVC runtime
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Dependencies
find_package(pugixml REQUIRED)

find_path(FCGI_INCLUDE_DIR NAMES fastcgi.h fcgio.h)

# Link FCGI
if(WIN32)
    find_library(FCGI_LIB NAMES fcgi libfcgi)
else()
    find_library(FCGI_LIB NAMES fcgi)
endif()

if(NOT FCGI_INCLUDE_DIR OR NOT FCGI_LIB)
    message(FATAL_ERROR
        "FastCGI not found!\n"
        "  Debian based: sudo apt install libfcgi-dev\n"
        "  Fedora based: sudo dnf install fcgi-devel\n"
        "  macOS: brew install fcgi\n"
    )
endif()

# LuaJIT
find_package(PkgConfig REQUIRED)
pkg_check_modules(LUAJIT REQUIRED IMPORTED_TARGET luajit)

# Prefer a static library
find_library(LUAJIT_STATIC_LIB
    NAMES libluajit-5.1.a libluajit.a luajit-5.1 luajit
    PATHS ${LUAJIT_LIBRARY_DIRS}
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH
)

if(NOT LUAJIT_STATIC_LIB)
    find_library(LUAJIT_STATIC_LIB NAMES libluajit-5.1.a libluajit.a luajit-5.1 luajit)
endif()

# Show a message if failed
if(NOT LUAJIT_STATIC_LIB)
    message(FATAL_ERROR
        "LuaJIT library not found!\n"
        "Install the static LuaJIT library:\n"
        "  Debian based: sudo apt install libluajit-5.1-dev\n"
        "  Fedora based: sudo dnf install luajit-devel\n"
        "  macOS: brew install luajit\n"
        "  Windows: winget install -e --id DEVCOM.LuaJIT"
    )
endif()

# Verify
if(NOT LUAJIT_STATIC_LIB MATCHES "\\.(a|lib)$")
    message(WARNING
        "Found LuaJIT but it may not be static: ${LUAJIT_STATIC_LIB}"
    )
endif()

message(STATUS "Using LuaJIT: ${LUAJIT_STATIC_LIB}")

# Source
add_executable(silvermoon
    src/main.cpp
    src/html/parser/parser.cpp
    src/html/parser/parser.h
    src/lua/manager/manager.cpp
    src/lua/manager/manager.h
    src/html/creator/creator.cpp
    src/html/creator/creator.h
    src/lua/api/api.cpp
    src/lua/api/api.h
    src/lua/transport/transport.h
    src/lua/transport/transport.cpp
)

# Link things
target_include_directories(silvermoon PRIVATE
    ${LUAJIT_INCLUDE_DIRS}
    ${FCGI_INCLUDE_DIR}
)

# Export symbols so LuaJIT's FFI and dlsym-based lookups work
set_target_properties(silvermoon PROPERTIES ENABLE_EXPORTS ON)

# Pugixml built and installed from source.
target_link_libraries(silvermoon PRIVATE
    pugixml::pugixml
    ${LUAJIT_STATIC_LIB}
    ${FCGI_LIB}
    ${CMAKE_DL_LIBS}
)

# LuaJIT needs libm on Linux
if(UNIX AND NOT APPLE)
    target_link_libraries(silvermoon PRIVATE m)
endif()

# Link the GCC runtime statically on Linux so the binary doesn't need it
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT APPLE)
    target_link_options(silvermoon PRIVATE -static-libgcc -static-libstdc++)
endif()

target_compile_definitions(silvermoon PRIVATE SM_VERSION="${PROJECT_VERSION}")
