cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if ("${CMAKE_SYSTEM_NAME}" STREQUAL "iOS" OR IOS)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
else()
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
endif()
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

project(GDAutopilot VERSION 1.0.0 LANGUAGES CXX)

# Core modules + Geode integration sources
set(AUTOPILOT_CORE_SOURCES
    src/core/DebugLog.cpp
    src/modules/PhysicsEngine.cpp
    src/modules/LevelAnalyzer.cpp
    src/modules/GameController.cpp
    src/modules/LearningSystem.cpp
    src/modules/AIEngine.cpp
    src/modules/StatisticsManager.cpp
)

set(AUTOPILOT_GEODE_SOURCES
    src/modules/AIPlayer.cpp
    src/modules/GeodeInterface.cpp
    src/main.cpp
)

add_library(${PROJECT_NAME} SHARED
    ${AUTOPILOT_CORE_SOURCES}
    ${AUTOPILOT_GEODE_SOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Unable to find Geode SDK! Please define GEODE_SDK environment variable to point to Geode")
else()
    message(STATUS "Found Geode: $ENV{GEODE_SDK}")
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

# Set up dependencies, resources, and link Geode.
setup_geode_mod(${PROJECT_NAME})
