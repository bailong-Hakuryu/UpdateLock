cmake_minimum_required(VERSION 3.14)

# Read version from version.h (kept in sync via the header itself)
project(UpdateLock VERSION 1.1.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Compiler warnings — strict Google C++ Style
# ---------------------------------------------------------------------------
if(MSVC)
    add_compile_options(/W4 /utf-8)
    # Use wmain entry point (Unicode argv) with MSVC subsystem:console
    add_link_options(/ENTRY:wmainCRTStartup)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -municode)
endif()

# ---------------------------------------------------------------------------
# Source files
# ---------------------------------------------------------------------------
set(SOURCES
    src/immunizer.h
    src/immunizer.cc
    src/main.cc
    src/process_manager.h
    src/process_manager.cc
    src/security_manager.h
    src/security_manager.cc
    src/version.h
    src/version.rc   # Windows version/metadata resource
)

add_executable(UpdateLock ${SOURCES})

target_include_directories(UpdateLock PRIVATE src)

target_link_libraries(UpdateLock PRIVATE advapi32 shell32 user32)

# ---------------------------------------------------------------------------
# Copy rules.json next to the built executable so it is always available.
# ---------------------------------------------------------------------------
add_custom_command(TARGET UpdateLock POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/rules.json"
        "$<TARGET_FILE_DIR:UpdateLock>/rules.json"
    COMMENT "Copying rules.json to output directory..."
)
