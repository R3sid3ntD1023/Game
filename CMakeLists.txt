cmake_minimum_required(VERSION 3.21)
project(Game LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(GAME_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(GAME_ENABLE_SANITIZERS "Enable AddressSanitizer" OFF)

# Configurations: multi-config generators get an explicit list, single-config defaults to RelWithDebInfo.
get_property(_is_multi GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(_is_multi)
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo" CACHE STRING "" FORCE)
elseif(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo)
endif()

find_package(glad CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(EnTT CONFIG REQUIRED)
find_package(cereal CONFIG REQUIRED)
find_package(OpenAL CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_path(STB_INCLUDE_DIRS "stb_image.h")

file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.cpp)
add_executable(Game ${GAME_SOURCES})

target_include_directories(Game PRIVATE src ${STB_INCLUDE_DIRS})
target_precompile_headers(Game PRIVATE ${CMAKE_SOURCE_DIR}/src/pch.h)
target_compile_definitions(Game PRIVATE
    SHADER_DIR="${CMAKE_SOURCE_DIR}/shaders/"
    ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets/"
    # Readable JSON worlds while developing, compact binary worlds in Release.
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:GAME_SCENE_JSON=1>
    $<$<CONFIG:Debug>:GAME_DEBUG=1>
)
target_link_libraries(Game PRIVATE
    glad::glad
    glfw
    glm::glm
    spdlog::spdlog
    EnTT::EnTT
    cereal::cereal
    OpenAL::OpenAL
    assimp::assimp
)

set_target_properties(Game PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin/RelWithDebInfo"
)
if(MSVC)
    set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Game)
endif()

if(MSVC)
    target_compile_options(Game PRIVATE /W4 /permissive- /utf-8)
    if(GAME_WARNINGS_AS_ERRORS)
        target_compile_options(Game PRIVATE /WX)
    endif()
    if(GAME_ENABLE_SANITIZERS)
        target_compile_options(Game PRIVATE /fsanitize=address)
    endif()
else()
    target_compile_options(Game PRIVATE -Wall -Wextra)
    if(GAME_WARNINGS_AS_ERRORS)
        target_compile_options(Game PRIVATE -Werror)
    endif()
    if(GAME_ENABLE_SANITIZERS)
        target_compile_options(Game PRIVATE -fsanitize=address -fno-omit-frame-pointer)
        target_link_options(Game PRIVATE -fsanitize=address)
    endif()
endif()
