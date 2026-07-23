cmake_minimum_required(VERSION 3.14)
project(UpdateLock VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable strict Google C++ Style compiler warnings
if(MSVC)
    add_compile_options(/W4 /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

file(GLOB SOURCES
    "src/*.h"
    "src/*.cc"
)

add_executable(UpdateLock ${SOURCES})

target_link_libraries(UpdateLock PRIVATE advapi32 shell32 user32)
