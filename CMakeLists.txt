cmake_minimum_required(VERSION 3.14)
project(walkk)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Override minimum CMake version for dependencies
set(CMAKE_POLICY_DEFAULT_CMP0000 NEW)

# Platform detection
if(WIN32)
    set(IS_WINDOWS TRUE)
else()
    set(IS_WINDOWS FALSE)
endif()

# Dependencies: PortAudio
if(IS_WINDOWS)
    # On Windows, try different PortAudio package names
    find_package(portaudio CONFIG QUIET)
    if(NOT portaudio_FOUND)
        find_package(PortAudio CONFIG QUIET)
    endif()
    if(NOT portaudio_FOUND AND NOT PortAudio_FOUND)
        # Fallback to PkgConfig on Windows if available
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)
        else()
            message(FATAL_ERROR "PortAudio not found. Install via: vcpkg install portaudio:x86-windows")
        endif()
    else()
        set(PORTAUDIO_LIBRARIES portaudio)
    endif()
else()
    # Linux: use pkg-config
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)
endif()

# Core library used by CLI and GUI
add_library(walkk_core
    src/audio_file.cpp
    src/pa_sink.cpp
    src/walkk.cpp
)
target_include_directories(walkk_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

if(IS_WINDOWS)
    target_link_libraries(walkk_core PUBLIC ${PORTAUDIO_LIBRARIES})
else()
    target_include_directories(walkk_core PUBLIC ${PORTAUDIO_INCLUDE_DIRS})
    target_link_libraries(walkk_core PUBLIC
        ${PORTAUDIO_LIBRARIES}
        pthread
    )
    target_compile_options(walkk_core PRIVATE ${PORTAUDIO_CFLAGS_OTHER})
endif()

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

# Set up ImGui sources based on platform
if(IS_WINDOWS)
    set(IMGUI_SOURCES
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
    )
else()
    set(IMGUI_SOURCES
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )
endif()

add_library(imgui_lib ${IMGUI_SOURCES})
target_include_directories(imgui_lib PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
# Link appropriate graphics libraries based on platform
if(IS_WINDOWS)
    target_link_libraries(imgui_lib PUBLIC glfw d3d11 dxgi)
else()
    target_link_libraries(imgui_lib PUBLIC glfw OpenGL::GL)
endif()

# GL loader (GLAD) - only needed on non-Windows platforms
if(NOT IS_WINDOWS)
    FetchContent_Declare(
        glad
        GIT_REPOSITORY https://github.com/Dav1dde/glad.git
        GIT_TAG v2.0.4
    )
    FetchContent_MakeAvailable(glad)
endif()

# Set appropriate compile definitions based on platform
if(IS_WINDOWS)
    target_compile_definitions(imgui_lib PUBLIC IMGUI_IMPL_WIN32 IMGUI_IMPL_DX11)
else()
    target_compile_definitions(imgui_lib PUBLIC IMGUI_IMPL_OPENGL_LOADER_GLAD)
endif()

add_executable(walkk_gui
    src/gui_main.cpp
    external/tinyfiledialogs/tinyfiledialogs.c
)
target_include_directories(walkk_gui PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/external/tinyfiledialogs)
# Link GUI target with appropriate libraries based on platform
if(IS_WINDOWS)
    target_link_libraries(walkk_gui PRIVATE walkk_core imgui_lib)
else()
    target_link_libraries(walkk_gui PRIVATE walkk_core imgui_lib glad)
    target_include_directories(walkk_gui PRIVATE ${glad_SOURCE_DIR}/include)
endif()

# Windows-specific settings
if(MSVC)
    # Set console subsystem for CLI, Windows subsystem for GUI
    set_target_properties(walkk_cli PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:CONSOLE"
    )
    set_target_properties(walkk_gui PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup"
        WIN32_EXECUTABLE TRUE
    )
    
    # Disable MSVC warnings about unsafe functions
    target_compile_definitions(walkk_core PRIVATE
        _CRT_SECURE_NO_WARNINGS
    )
    
    # Enable multi-processor compilation
    target_compile_options(walkk_core PRIVATE /MP)
endif()

# ---- INSTALLATION ----
# Build shared by default on Linux, static on Windows (typical convention)
if(NOT DEFINED BUILD_SHARED_LIBS)
    if(IS_WINDOWS)
        option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
    else()
        option(BUILD_SHARED_LIBS "Build shared libraries" ON)
    endif()
endif()

# Public headers
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION include)

# Platform-specific installation paths
if(IS_WINDOWS)
    set(INSTALL_BIN_DIR bin)
    set(INSTALL_LIB_DIR lib)
else()
    set(INSTALL_BIN_DIR bin)
    set(INSTALL_LIB_DIR lib64)
endif()

# Binaries and library
install(TARGETS
    walkk_cli
    walkk_gui
    walkk_core
    imgui_lib
    RUNTIME DESTINATION ${INSTALL_BIN_DIR}
    LIBRARY DESTINATION ${INSTALL_LIB_DIR}
    ARCHIVE DESTINATION ${INSTALL_LIB_DIR}
)

# Linux-specific desktop integration
if(NOT IS_WINDOWS)
    # Desktop entry for the GUI app
    if(EXISTS ${CMAKE_SOURCE_DIR}/packaging/walkk-gui.desktop)
        install(FILES
            ${CMAKE_SOURCE_DIR}/packaging/walkk-gui.desktop
            DESTINATION share/applications
        )
    endif()

    # Icon
    if(EXISTS ${CMAKE_SOURCE_DIR}/packaging/icons/walkk.png)
        install(FILES 
            ${CMAKE_SOURCE_DIR}/packaging/icons/walkk.png 
            DESTINATION share/icons/hicolor/256x256/apps
        )
    endif()
endif()

# ---- CPACK (RPM for Linux, NSIS/WIX for Windows) ----
include(CPack)

if(IS_WINDOWS)
    # Windows installer (NSIS or WIX)
    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_NSIS_DISPLAY_NAME "Walkk MP3 Player")
    set(CPACK_NSIS_PACKAGE_NAME "Walkk")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MODIFY_PATH ON)
    # Create start menu shortcuts
    set(CPACK_NSIS_MENU_LINKS
        "bin/walkk_gui.exe" "Walkk Player"
    )
else()
    # Linux RPM packaging
    set(CPACK_GENERATOR RPM)
    set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
    # Runtime deps (adjust names to your distro)
    set(CPACK_RPM_PACKAGE_REQUIRES "portaudio, glfw")
    set(CPACK_RPM_PACKAGE_LICENSE "MIT")
    
    # Optional: split devel files
    set(CPACK_COMPONENTS_ALL runtime devel)
    set(CPACK_COMPONENT_RUNTIME_DISPLAY_NAME "Runtime")
    cpack_add_component(runtime)
    cpack_add_component(devel)
    
    # Mark which installed paths belong to which component
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION include COMPONENT devel)
endif()

# Common CPack settings
set(CPACK_PACKAGE_NAME "walkk")
set(CPACK_PACKAGE_VENDOR "YourOrg")
set(CPACK_PACKAGE_CONTACT "you@example.com")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "MP3 player with CLI and ImGui/GLFW GUI")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://example.com")
set(CPACK_PACKAGE_VERSION "1.0.0")