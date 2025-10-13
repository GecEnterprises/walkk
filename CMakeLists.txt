cmake_minimum_required(VERSION 3.10)
project(MP3Player)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find PortAudio
find_package(PkgConfig REQUIRED)
pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)

# Include directories
include_directories(${PORTAUDIO_INCLUDE_DIRS})
include_directories(${CMAKE_SOURCE_DIR}/include)

# Add executable
add_executable(mp3player
    src/main.cpp
)

# Link libraries
target_link_libraries(mp3player
    ${PORTAUDIO_LIBRARIES}
    pthread
)

# Set compile options
target_compile_options(mp3player PRIVATE ${PORTAUDIO_CFLAGS_OTHER})