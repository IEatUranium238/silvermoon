cmake_minimum_required(VERSION 3.10.0)

project(silvermoon VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(pugixml REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(LUAJIT REQUIRED luajit)
pkg_check_modules(TIDY REQUIRED tidy)

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
    ${TIDY_INCLUDE_DIRS}
)

target_link_directories(silvermoon PRIVATE
    ${LUAJIT_LIBRARY_DIRS}
    ${TIDY_LIBRARY_DIRS}
)

target_link_libraries(silvermoon PRIVATE
    pugixml::pugixml
    ${LUAJIT_LIBRARIES}
    ${TIDY_LIBRARIES}
)