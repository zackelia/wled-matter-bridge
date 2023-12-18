cmake_minimum_required(VERSION 3.22)

project(wled-matter-bridge
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS On)

add_subdirectory(src)
