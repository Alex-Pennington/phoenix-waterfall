# Phoenix Waterfall - CMake Build System

cmake_minimum_required(VERSION 3.16)

project(phoenix-waterfall
    VERSION 0.3.1
    DESCRIPTION "Phoenix SDR Waterfall Display"
    LANGUAGES C
)

# Phoenix Build System integration
include(external/phoenix-build-scripts/cmake/phoenix-build.cmake)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra)
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

#============================================================================
# Dependencies - SDL2 + SDL2_ttf
# Try bundled first (libs/), fall back to system (for CI)
#============================================================================

set(SDL2_LOCAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/SDL2/SDL2-2.30.9/x86_64-w64-mingw32")
set(SDL2_TTF_LOCAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/SDL2_ttf/SDL2_ttf-2.22.0/x86_64-w64-mingw32")

# SDL2
if(EXISTS "${SDL2_LOCAL_DIR}/include/SDL2/SDL.h")
    message(STATUS "Using bundled SDL2 from libs/")
    set(SDL2_INCLUDE_DIRS "${SDL2_LOCAL_DIR}/include/SDL2")
    set(SDL2_LIBRARY_DIR "${SDL2_LOCAL_DIR}/lib")
    set(SDL2_DLL "${SDL2_LOCAL_DIR}/bin/SDL2.dll")
    set(SDL2_FOUND TRUE)
    set(SDL2_BUNDLED TRUE)
else()
    # Try system SDL2 (MSYS2, etc.)
    find_package(SDL2 QUIET)
    if(SDL2_FOUND)
        message(STATUS "Using system SDL2")
        set(SDL2_BUNDLED FALSE)
    else()
        message(FATAL_ERROR "SDL2 not found - install via MSYS2 or extract to libs/SDL2")
    endif()
endif()

# SDL2_ttf
if(EXISTS "${SDL2_TTF_LOCAL_DIR}/include/SDL2/SDL_ttf.h")
    message(STATUS "Using bundled SDL2_ttf from libs/")
    set(SDL2_TTF_INCLUDE_DIRS "${SDL2_TTF_LOCAL_DIR}/include/SDL2")
    set(SDL2_TTF_LIBRARY_DIR "${SDL2_TTF_LOCAL_DIR}/lib")
    set(SDL2_TTF_DLL "${SDL2_TTF_LOCAL_DIR}/bin/SDL2_ttf.dll")
    set(SDL2_TTF_FOUND TRUE)
    set(SDL2_TTF_BUNDLED TRUE)
else()
    # Try system SDL2_ttf
    find_package(SDL2_ttf QUIET)
    if(SDL2_ttf_FOUND)
        message(STATUS "Using system SDL2_ttf")
        set(SDL2_TTF_FOUND TRUE)
        set(SDL2_TTF_BUNDLED FALSE)
    else()
        message(WARNING "SDL2_ttf not found - GUI disabled")
        set(SDL2_TTF_FOUND FALSE)
    endif()
endif()

#============================================================================
# Phoenix Submodules
#============================================================================

# Phoenix Discovery
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/phoenix-discovery/CMakeLists.txt")
    add_subdirectory(external/phoenix-discovery)
    message(STATUS "Using phoenix-discovery from external/")
else()
    message(FATAL_ERROR "phoenix-discovery not found - run: git submodule update --init")
endif()

# Phoenix DSP
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/phoenix-dsp/CMakeLists.txt")
    add_subdirectory(external/phoenix-dsp)
    message(STATUS "Using phoenix-dsp from external/")
else()
    message(FATAL_ERROR "phoenix-dsp not found - run: git submodule update --init")
endif()

# Phoenix KISS FFT
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/phoenix-kiss-fft/CMakeLists.txt")
    add_subdirectory(external/phoenix-kiss-fft)
    message(STATUS "Using phoenix-kiss-fft from external/")
else()
    message(FATAL_ERROR "phoenix-kiss-fft not found - run: git submodule update --init")
endif()

#============================================================================
# Source files
#============================================================================

set(WATERFALL_SOURCES
    src/waterfall.c
    src/waterfall_audio.c
)

if(SDL2_TTF_FOUND)
    list(APPEND WATERFALL_SOURCES
        src/ui_core.c
        src/ui_widgets.c
    )
endif()

#============================================================================
# Executable
#============================================================================

add_executable(waterfall ${WATERFALL_SOURCES})

if(SDL2_TTF_FOUND)
    target_compile_definitions(waterfall PRIVATE HAS_GUI=1)
endif()

target_include_directories(waterfall PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# SDL2 includes and linking
if(SDL2_BUNDLED)
    target_include_directories(waterfall PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_directories(waterfall PRIVATE ${SDL2_LIBRARY_DIR})
    target_link_libraries(waterfall PRIVATE mingw32 SDL2main SDL2)
else()
    target_link_libraries(waterfall PRIVATE SDL2::SDL2main SDL2::SDL2)
endif()

# SDL2_ttf includes and linking
if(SDL2_TTF_FOUND)
    if(SDL2_TTF_BUNDLED)
        target_include_directories(waterfall PRIVATE ${SDL2_TTF_INCLUDE_DIRS})
        target_link_directories(waterfall PRIVATE ${SDL2_TTF_LIBRARY_DIR})
        target_link_libraries(waterfall PRIVATE SDL2_ttf)
    else()
        target_link_libraries(waterfall PRIVATE SDL2_ttf::SDL2_ttf)
    endif()
endif()

# Phoenix submodule libraries
target_link_libraries(waterfall PRIVATE 
    pn_discovery 
    pn_dsp 
    pn_kiss_fft
)

if(WIN32)
    target_link_libraries(waterfall PRIVATE ws2_32 winmm iphlpapi)
endif()

if(NOT WIN32)
    target_link_libraries(waterfall PRIVATE m)
endif()

#============================================================================
# Copy DLLs to output (Windows, bundled SDL2 only)
#============================================================================

if(WIN32 AND SDL2_BUNDLED AND EXISTS "${SDL2_DLL}")
    add_custom_command(TARGET waterfall POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SDL2_DLL}" "$<TARGET_FILE_DIR:waterfall>"
        COMMENT "Copying SDL2.dll"
    )
endif()

if(WIN32 AND SDL2_TTF_BUNDLED AND EXISTS "${SDL2_TTF_DLL}")
    add_custom_command(TARGET waterfall POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SDL2_TTF_DLL}" "$<TARGET_FILE_DIR:waterfall>"
        COMMENT "Copying SDL2_ttf.dll"
    )
endif()

#============================================================================
# Summary
#============================================================================

message(STATUS "")
message(STATUS "Phoenix Waterfall v${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  SDL2_ttf:   ${SDL2_TTF_FOUND}")
message(STATUS "")
