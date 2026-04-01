cmake_minimum_required(VERSION 3.14)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(userver-service CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "CMAKE_MODULE_PATH = ${CMAKE_MODULE_PATH}")

add_subdirectory(utfcpp)

find_package(
    userver
    COMPONENTS core redis sqlite
    REQUIRED
)
find_package(sodium REQUIRED)

add_executable(${PROJECT_NAME}
    main.cpp
    game/ScrabbleGame.cpp
    game/Storage.cpp
    api/sqlite.cpp
    api/websocket.cpp
    api/http.cpp
    api/Cors.cpp
)
target_link_libraries(${PROJECT_NAME}
    userver::core
    userver::redis
    userver::sqlite
    sodium
    utf8cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_MODULE_PATH}
)
