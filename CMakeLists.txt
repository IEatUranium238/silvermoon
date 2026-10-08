cmake_minimum_required(VERSION 3.10.0)

project(silvermoon VERSION 0.4.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Prefer static libs
if(NOT WIN32)
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a" ${CMAKE_FIND_LIBRARY_SUFFIXES})
endif()

if(WIN32)
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".lib" ".a" ${CMAKE_FIND_LIBRARY_SUFFIXES})
endif()

# Link MSVC
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Pugixml built and installed from source.
find_package(pugixml REQUIRED)

find_path(FCGI_INCLUDE_DIR NAMES fastcgi.h fcgio.h)

# Link FCGI
if(WIN32)
    find_library(FCGI NAMES fcgi libfcgi)
else()
    find_library(FCGI NAMES fcgi)
endif()

# LUAJIT
find_package(PkgConfig REQUIRED)
pkg_check_modules(LUAJIT REQUIRED luajit)

# Search for luajit
find_library(LUAJIT_STATIC_LIB
    NAMES luajit-5.1 luajit
    PATHS ${LUAJIT_LIBRARY_DIRS} /usr/lib /usr/local/lib /opt/local/lib
    NO_DEFAULT_PATH
)

if(NOT LUAJIT_STATIC_LIB)
    find_library(LUAJIT_STATIC_LIB NAMES luajit-5.1 luajit)
endif()

# Show a message if failed
if(NOT LUAJIT_STATIC_LIB)
    message(FATAL_ERROR
        "Static LuaJIT library not found!\n"
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
        "Found LuaJIT library but it might not be static: ${LUAJIT_STATIC_LIB}\n"
        "Consider installing libluajit-5.1-dev package for static libs"
    )
endif()

message(STATUS "Using static LuaJIT library: ${LUAJIT_STATIC_LIB}")
# LUAJIT - END

# Source
add_executable(silvermoon
    ./src/main.cpp
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

target_link_directories(silvermoon PRIVATE
    ${LUAJIT_LIBRARY_DIRS}
)

# Pugixml built and installed from source.
target_link_libraries(silvermoon PRIVATE
    pugixml::pugixml
    ${LUAJIT_STATIC_LIB}
    ${FCGI}
)

# Link gcc runtime on Linux so the binary wont need a lib
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT APPLE)
    target_link_options(silvermoon PRIVATE -static-libgcc -static-libstdc++)
endif()

target_compile_definitions(silvermoon PRIVATE SM_VERSION="${PROJECT_VERSION}")
