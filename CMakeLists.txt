cmake_minimum_required(VERSION 3.21)
project(CustomBGMusic VERSION 1.0.0)

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Please define GEODE_SDK environment variable to point to the Geode SDK (see https://docs.geode-sdk.org)")
else()
    message(STATUS "Found Geode SDK: $ENV{GEODE_SDK}")
    set(GEODE_SDK_PATH $ENV{GEODE_SDK})
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

file(GLOB SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

add_library(${PROJECT_NAME} SHARED ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

setup_geode_mod(${PROJECT_NAME})
