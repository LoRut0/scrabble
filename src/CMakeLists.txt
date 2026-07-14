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
    game/GameViewBuilder.cpp
    game/ScrabbleGame.cpp
    api/Cors.cpp
    api/http.cpp
    api/sqlite.cpp
    api/websocket.cpp
    session/GameRoom.cpp
    session/GameStorage.cpp
    session/PlayerSession.cpp
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

option(ENABLE_ASAN "Build userver-service with AddressSanitizer" OFF)
if(ENABLE_ASAN)
    target_compile_options(${PROJECT_NAME} PRIVATE -fsanitize=address -fno-omit-frame-pointer -g)
    target_link_options(${PROJECT_NAME} PRIVATE -fsanitize=address)
endif()

userver_testsuite_add(
    SERVICE_TARGET ${PROJECT_NAME}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../tests
    PYTEST_ARGS
        --service-config=${CMAKE_CURRENT_SOURCE_DIR}/../static_config.yaml
        --service-binary=${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}
        --service-shutdown-timeout=10
)
