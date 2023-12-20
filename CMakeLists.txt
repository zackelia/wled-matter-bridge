cmake_minimum_required(VERSION 3.22)

project(wled-matter-bridge-deps
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS On)

set(ENABLE_WEBSOCKETS ON)
set(BUILD_STATIC_LIBS ON)
add_subdirectory(third_party/curl)

set(CJSON_OVERRIDE_BUILD_SHARED_LIBS ON)
set(CJSON_BUILD_SHARED_LIBS OFF)
add_subdirectory(third_party/cJSON)
