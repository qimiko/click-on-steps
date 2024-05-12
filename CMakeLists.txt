cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_ARCHITECTURES "x86_64")
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

project(cbf-lite VERSION 1.0.0)

set(MOD_SOURCES
    src/main.cpp
    src/async.cpp
    src/input.cpp
)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    set(MOD_SOURCES ${MOD_SOURCES} src/macos.mm)
    set_source_files_properties(src/macos.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Android")
    set(MOD_SOURCES ${MOD_SOURCES} src/android.cpp)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    set(MOD_SOURCES ${MOD_SOURCES} src/windows.cpp)
else()
    message(FATAL_ERROR "Unsupported mod platform :(")
endif()

add_library(${PROJECT_NAME} SHARED ${MOD_SOURCES})

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Unable to find Geode SDK! Please define GEODE_SDK environment variable to point to Geode")
else()
    message(STATUS "Found Geode: $ENV{GEODE_SDK}")
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

setup_geode_mod(${PROJECT_NAME})
