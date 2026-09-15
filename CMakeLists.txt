cmake_minimum_required(VERSION 3.10.0)

project(silvermoon VERSION 0.1.5 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(pugixml REQUIRED)
find_package(PkgConfig REQUIRED)

find_library(FCGI NAMES fcgi)
find_path(FCGI_INCLUDE_DIR NAMES fastcgi.h fcgio.h)

pkg_check_modules(LUAJIT REQUIRED luajit)

add_executable(silvermoon
    ./src/main.cpp
    src/html/parser/parser.cpp
    src/html/parser/parser.h
    src/lua/manager/manager.cpp
    src/lua/manager/manager.h
    src/html/creator/creator.cpp
    src/html/creator/creator.h
)

target_include_directories(silvermoon PRIVATE
    ${LUAJIT_INCLUDE_DIRS}
    ${FCGI_INCLUDE_DIR}
)

target_link_directories(silvermoon PRIVATE
    ${LUAJIT_LIBRARY_DIRS}
)

target_link_libraries(silvermoon PRIVATE 
    pugixml::pugixml
    ${LUAJIT_LIBRARIES}
    ${FCGI}
    ${FCGI_C_LIBRARY}
)

target_compile_definitions(silvermoon PRIVATE SM_VERSION="${PROJECT_VERSION}")