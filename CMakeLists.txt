cmake_minimum_required(VERSION 3.10.0)

project(silvermoon VERSION 0.1.6 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Prefer static libs on UNIX like
if(NOT WIN32)
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a" ${CMAKE_FIND_LIBRARY_SUFFIXES})
endif()

# Link MSVC
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Pugixml built and installed from source.
find_package(pugixml REQUIRED)
find_package(PkgConfig REQUIRED)

find_path(FCGI_INCLUDE_DIR NAMES fastcgi.h fcgio.h)

# Link FCGI
if(WIN32)
    find_library(FCGI NAMES fcgi libfcgi)
else()
    find_library(FCGI NAMES fcgi)
endif()

# Find luajit
pkg_check_modules(LUAJIT REQUIRED luajit)

# Prefer static libs if found
if(LUAJIT_STATIC_FOUND OR LUAJIT_STATIC_LIBRARIES)
    set(LUAJIT_LINK_LIBRARIES ${LUAJIT_STATIC_LIBRARIES})
    set(LUAJIT_LINK_DIRS ${LUAJIT_STATIC_LIBRARY_DIRS})
    set(LUAJIT_LINK_INCLUDE_DIRS ${LUAJIT_STATIC_INCLUDE_DIRS})
else()
    message(WARNING "Static LuaJIT libs not found via pkg-config! Falling back to shared. "
                     "Install a static luajit .a/.lib or a vcpkg static triplet to fix this.")

    set(LUAJIT_LINK_LIBRARIES ${LUAJIT_LIBRARIES})
    set(LUAJIT_LINK_DIRS ${LUAJIT_LIBRARY_DIRS})
    set(LUAJIT_LINK_INCLUDE_DIRS ${LUAJIT_INCLUDE_DIRS})
endif()

# Add source files
add_executable(silvermoon
    ./src/main.cpp
    src/html/parser/parser.cpp
    src/html/parser/parser.h
    src/lua/manager/manager.cpp
    src/lua/manager/manager.h
    src/html/creator/creator.cpp
    src/html/creator/creator.h
)

# Link things
target_include_directories(silvermoon PRIVATE
    ${LUAJIT_LINK_INCLUDE_DIRS}
    ${FCGI_INCLUDE_DIR}
)

target_link_directories(silvermoon PRIVATE
    ${LUAJIT_LINK_DIRS}
)

# Pugixml built and installed from source.
target_link_libraries(silvermoon PRIVATE
    pugixml::pugixml
    ${LUAJIT_LINK_LIBRARIES}
    ${FCGI}
)

# Link gcc runtime on Linux so the binary wont need a lib
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT APPLE)
    target_link_options(silvermoon PRIVATE -static-libgcc -static-libstdc++)
endif()

target_compile_definitions(silvermoon PRIVATE SM_VERSION="${PROJECT_VERSION}")