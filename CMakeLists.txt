# Phoenix Waterfall - CMake Build System

cmake_minimum_required(VERSION 3.16)

project(phoenix-waterfall
    VERSION 0.1.0
    DESCRIPTION "Phoenix SDR Waterfall Display"
    LANGUAGES C
)

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
# Dependencies - SDL2 + SDL2_ttf (bundled locally)
#============================================================================

set(SDL2_LOCAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/SDL2/SDL2-2.30.9/x86_64-w64-mingw32")
set(SDL2_TTF_LOCAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/SDL2_ttf/SDL2_ttf-2.22.0/x86_64-w64-mingw32")

if(EXISTS "${SDL2_LOCAL_DIR}/include/SDL2/SDL.h")
    message(STATUS "Using local SDL2 from libs/")
    set(SDL2_INCLUDE_DIRS "${SDL2_LOCAL_DIR}/include/SDL2")
    set(SDL2_LIBRARY_DIR "${SDL2_LOCAL_DIR}/lib")
    set(SDL2_DLL "${SDL2_LOCAL_DIR}/bin/SDL2.dll")
    set(SDL2_FOUND TRUE)
else()
    message(FATAL_ERROR "SDL2 not found in libs/SDL2")
endif()

if(EXISTS "${SDL2_TTF_LOCAL_DIR}/include/SDL2/SDL_ttf.h")
    message(STATUS "Using local SDL2_ttf from libs/")
    set(SDL2_TTF_INCLUDE_DIRS "${SDL2_TTF_LOCAL_DIR}/include/SDL2")
    set(SDL2_TTF_LIBRARY_DIR "${SDL2_TTF_LOCAL_DIR}/lib")
    set(SDL2_TTF_DLL "${SDL2_TTF_LOCAL_DIR}/bin/SDL2_ttf.dll")
    set(SDL2_TTF_FOUND TRUE)
else()
    message(WARNING "SDL2_ttf not found - GUI disabled")
    set(SDL2_TTF_FOUND FALSE)
endif()

#============================================================================
# Phoenix Discovery (submodule)
#============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/phoenix-discovery/CMakeLists.txt")
    add_subdirectory(external/phoenix-discovery)
    set(DISCOVERY_FOUND TRUE)
    message(STATUS "Using phoenix-discovery from external/")
else()
    message(FATAL_ERROR "phoenix-discovery not found - run: git submodule update --init")
endif()

#============================================================================
# Source files
#============================================================================

set(WATERFALL_SOURCES
    src/waterfall.c
    src/kiss_fft.c
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
    ${CMAKE_CURRENT_SOURCE_DIR}/external/phoenix-discovery/include
    ${SDL2_INCLUDE_DIRS}
)

if(SDL2_TTF_FOUND)
    target_include_directories(waterfall PRIVATE ${SDL2_TTF_INCLUDE_DIRS})
endif()

target_link_directories(waterfall PRIVATE ${SDL2_LIBRARY_DIR})
if(SDL2_TTF_FOUND)
    target_link_directories(waterfall PRIVATE ${SDL2_TTF_LIBRARY_DIR})
endif()

target_link_libraries(waterfall PRIVATE mingw32 SDL2main SDL2 pn_discovery)
if(SDL2_TTF_FOUND)
    target_link_libraries(waterfall PRIVATE SDL2_ttf)
endif()

if(WIN32)
    target_link_libraries(waterfall PRIVATE ws2_32 winmm iphlpapi)
endif()

if(NOT WIN32)
    target_link_libraries(waterfall PRIVATE m)
endif()

#============================================================================
# Copy DLLs to output (Windows)
#============================================================================

if(WIN32 AND EXISTS "${SDL2_DLL}")
    add_custom_command(TARGET waterfall POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SDL2_DLL}" "$<TARGET_FILE_DIR:waterfall>"
        COMMENT "Copying SDL2.dll"
    )
endif()

if(WIN32 AND SDL2_TTF_FOUND AND EXISTS "${SDL2_TTF_DLL}")
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
