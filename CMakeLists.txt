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
find_package(imgui CONFIG REQUIRED)
find_package(imguizmo CONFIG REQUIRED)
find_path(STB_INCLUDE_DIRS "stb_image.h")

set(SRC ${CMAKE_SOURCE_DIR}/src)

# Engine: everything that is not game-specific or editor-specific.
file(GLOB_RECURSE ENG_SOURCES CONFIGURE_DEPENDS
    ${SRC}/ai/*.cpp ${SRC}/audio/*.cpp ${SRC}/core/*.cpp ${SRC}/ecs/*.cpp
    ${SRC}/eng/*.cpp ${SRC}/gfx/*.cpp ${SRC}/ui/*.cpp ${SRC}/pch.cpp)
file(GLOB_RECURSE ENG_HEADERS CONFIGURE_DEPENDS
    ${SRC}/ai/*.h ${SRC}/audio/*.h ${SRC}/core/*.h ${SRC}/ecs/*.h
    ${SRC}/eng/*.h ${SRC}/gfx/*.h ${SRC}/ui/*.h ${SRC}/pch.h)
file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS ${SRC}/game/*.cpp)
file(GLOB_RECURSE GAME_HEADERS CONFIGURE_DEPENDS ${SRC}/game/*.h)
file(GLOB_RECURSE EDITOR_SOURCES CONFIGURE_DEPENDS ${SRC}/editor/*.cpp)
file(GLOB_RECURSE EDITOR_HEADERS CONFIGURE_DEPENDS ${SRC}/editor/*.h)

add_library(eng STATIC ${ENG_SOURCES} ${ENG_HEADERS})
target_include_directories(eng PUBLIC ${SRC} ${STB_INCLUDE_DIRS})
target_precompile_headers(eng PRIVATE ${SRC}/pch.h)
target_compile_definitions(eng PUBLIC
    SHADER_DIR="${CMAKE_SOURCE_DIR}/shaders/"
    ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets/"
    # Readable JSON worlds while developing, compact binary worlds in Release.
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:GAME_SCENE_JSON=1>
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:_DEVELOPMENT=1>
    $<$<CONFIG:Debug>:GAME_DEBUG=1>
)
target_link_libraries(eng PUBLIC
    glad::glad
    glfw
    glm::glm
    spdlog::spdlog
    EnTT::EnTT
    cereal::cereal
    OpenAL::OpenAL
    assimp::assimp
)

add_executable(Game ${SRC}/main.cpp ${GAME_SOURCES} ${GAME_HEADERS})
target_link_libraries(Game PRIVATE eng)
target_precompile_headers(Game REUSE_FROM eng)

# The editor reuses the game's content (generators, AI, scripts) to preview scenes and to play/simulate them.
set(EDITOR_GAME_SOURCES ${GAME_SOURCES})
list(FILTER EDITOR_GAME_SOURCES EXCLUDE REGEX "/game/(Game|MenuScreens)\\.cpp$")
add_executable(Editor ${EDITOR_SOURCES} ${EDITOR_HEADERS} ${EDITOR_GAME_SOURCES})
target_link_libraries(Editor PRIVATE eng imgui::imgui imguizmo::imguizmo)
if(WIN32)
  target_link_libraries(Editor PRIVATE comdlg32)
endif()
target_precompile_headers(Editor REUSE_FROM eng)

foreach(tgt Game Editor)
    set_target_properties(${tgt} PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/bin/Debug"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin/Release"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_SOURCE_DIR}/bin/RelWithDebInfo"
    )
endforeach()
if(MSVC)
    set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Game)
endif()

foreach(tgt eng Game Editor)
    if(MSVC)
        target_compile_options(${tgt} PRIVATE /W4 /permissive- /utf-8)
        if(GAME_WARNINGS_AS_ERRORS)
            target_compile_options(${tgt} PRIVATE /WX)
        endif()
        if(GAME_ENABLE_SANITIZERS)
            target_compile_options(${tgt} PRIVATE /fsanitize=address)
        endif()
    else()
        target_compile_options(${tgt} PRIVATE -Wall -Wextra)
        if(GAME_WARNINGS_AS_ERRORS)
            target_compile_options(${tgt} PRIVATE -Werror)
        endif()
        if(GAME_ENABLE_SANITIZERS)
            target_compile_options(${tgt} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
            target_link_options(${tgt} PRIVATE -fsanitize=address)
        endif()
    endif()
endforeach()
