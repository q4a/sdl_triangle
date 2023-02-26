cmake_minimum_required(VERSION 3.18)
project(hlsl_compile)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")

if (NOT WIN32)
option(USE_CONAN "Use Conan build system" OFF)
option(USE_NINE "Use Gallium Nine for native D3D9 API" ON)
endif()

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "" FORCE)
endif()

### Set up output paths
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/bin)

if (WIN32)
message("Using Windows HLSL compiler")
else()
message("Using VKD3D HLSL compiler")

include(ProcessorCount)
ProcessorCount(N)
message("ProcessorCount = ${N}")

include(ExternalProject)
ExternalProject_Add(vkd3d
    GIT_REPOSITORY    https://gitlab.winehq.org/wine/vkd3d.git
    GIT_TAG           b1e13d6e33dd2be6a4b127d6c75fd088c5d6edee
    GIT_SHALLOW       ON
    BUILD_ALWAYS      OFF
    BUILD_IN_SOURCE   ON
    CONFIGURE_HANDLED_BY_BUILD ON
    CONFIGURE_COMMAND ./autogen.sh && ./configure
    BUILD_COMMAND     make -j${N}
    INSTALL_COMMAND   ""
)
ExternalProject_Get_property(vkd3d SOURCE_DIR BINARY_DIR)
set(VKD3D_INCLUDE_DIRS
    "${SOURCE_DIR}/include"
)
set(VKD3D_LIB ${BINARY_DIR}/.libs/libvkd3d-utils.so)
add_custom_target(dependencies ALL DEPENDS vkd3d)
endif() # WIN32

# Source related part

set(SRC_FILES
    "src/hlsl_compile.cpp"
)

if (MSVC)
    add_compile_options(/std:c++latest) # for filesystem
else()
    add_compile_options(-std=c++20) # for filesystem
endif()

add_executable(${PROJECT_NAME} WIN32 ${SRC_FILES})

if (NOT WIN32)
    set(SYSTEM_DEPS "${VKD3D_LIB}")
    target_include_directories(${PROJECT_NAME} PRIVATE
        "${VKD3D_INCLUDE_DIRS}"
    )
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${SYSTEM_DEPS}
)

configure_file(${PROJECT_SOURCE_DIR}/src/min.vs ${PROJECT_BINARY_DIR}/bin/min.vs COPYONLY)
configure_file(${PROJECT_SOURCE_DIR}/src/min.ps ${PROJECT_BINARY_DIR}/bin/min.ps COPYONLY)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
