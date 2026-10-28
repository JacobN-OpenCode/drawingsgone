cmake_minimum_required(VERSION 3.20)
project(DrawingsGone CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static MSVC runtime: a single .exe with no redistributable to install.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

add_executable(DrawingsGone WIN32
    src/main.cpp
    src/overlay.cpp
    src/selector.cpp
    src/settings.cpp
    res/app.rc
)

target_include_directories(DrawingsGone PRIVATE src)

target_link_libraries(DrawingsGone PRIVATE
    user32
    gdi32
    shell32
    advapi32
    msimg32
)

target_compile_definitions(DrawingsGone PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)

if (MSVC)
    target_compile_options(DrawingsGone PRIVATE /W4 /permissive- /utf-8)
    # res/app.rc already embeds res/app.manifest; stop the linker adding another.
    target_link_options(DrawingsGone PRIVATE /MANIFEST:NO)
else()
    target_compile_options(DrawingsGone PRIVATE -Wall -Wextra -static -static-libgcc -static-libstdc++)
    target_link_options(DrawingsGone PRIVATE -static -mwindows)
endif()
