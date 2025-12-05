cmake_minimum_required(VERSION 3.14)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(userver-service CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "CMAKE_MODULE_PATH = ${CMAKE_MODULE_PATH}")

find_package(
    userver
    COMPONENTS core redis sqlite
    REQUIRED
)
find_package(sodium REQUIRED)

add_executable(${PROJECT_NAME}
    main.cpp
    ScrabbleGame.cpp
    redis.cpp
    sqlite.cpp
    websocket.cpp
    http.cpp
)
target_link_libraries(${PROJECT_NAME}
    userver::core
    userver::redis
    userver::sqlite
    sodium
)

userver_testsuite_add_simple()
