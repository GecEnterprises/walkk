cmake_minimum_required(VERSION 3.14)
project(MP3Player)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies: PortAudio
find_package(PkgConfig REQUIRED)
pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)

# Core library used by CLI and GUI
add_library(walkk_core
    src/audio_file.cpp
    src/pa_sink.cpp
    src/walkk.cpp
)
target_include_directories(walkk_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${PORTAUDIO_INCLUDE_DIRS}
)
target_link_libraries(walkk_core PUBLIC
    ${PORTAUDIO_LIBRARIES}
    pthread
)
target_compile_options(walkk_core PRIVATE ${PORTAUDIO_CFLAGS_OTHER})

# CLI target
add_executable(walkk_cli
    src/main.cpp
)
target_link_libraries(walkk_cli PRIVATE walkk_core)

# GUI target with ImGui + GLFW
include(FetchContent)

set(IMGUI_TAG v1.91.0)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG ${IMGUI_TAG}
)

set(GLFW_TAG 3.3.9)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG ${GLFW_TAG}
)

FetchContent_MakeAvailable(imgui glfw)

find_package(OpenGL REQUIRED)

set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

add_library(imgui_lib ${IMGUI_SOURCES})
target_include_directories(imgui_lib PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui_lib PUBLIC glfw OpenGL::GL)

# GL loader (GLAD)
FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG v0.1.36
)
FetchContent_MakeAvailable(glad)

target_compile_definitions(imgui_lib PUBLIC IMGUI_IMPL_OPENGL_LOADER_GLAD)

add_executable(walkk_gui
    src/gui_main.cpp
)
target_include_directories(walkk_gui PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(walkk_gui PRIVATE walkk_core imgui_lib glad)