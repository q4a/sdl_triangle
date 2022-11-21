cmake_minimum_required(VERSION 3.18)
project(sdl_d3d9_triangle)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")
include(conan)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "" FORCE)
endif()

### Set up output paths
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Release)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Debug)

conan_cmake_run(CONANFILE conanfile.py
    BASIC_SETUP CMAKE_TARGETS
    BUILD missing
)

if (WIN32)
    # Nothing special
else() # for DXVK
    message("Using DXVK-native for D3D9 API")

    include(ExternalProject)
    ExternalProject_Add(dxvk-native
        GIT_REPOSITORY    https://github.com/Joshua-Ashton/dxvk-native
        GIT_TAG           a2dc99c407340432d4ba5bfa29efa685c27942ea
        GIT_SHALLOW       ON
        BUILD_ALWAYS      OFF
        CONFIGURE_HANDLED_BY_BUILD ON
        CONFIGURE_COMMAND meson ../dxvk-native
        BUILD_COMMAND     ninja
        INSTALL_COMMAND   ""
    )
    ExternalProject_Get_property(dxvk-native SOURCE_DIR BINARY_DIR)
    set(DXVK_NATIVE_INCLUDE_DIRS
        "${SOURCE_DIR}/include/native/directx"
        "${SOURCE_DIR}/include/native/windows"
    )
    set(DXVK_NATIVE_D3D9_LIB ${BINARY_DIR}/src/d3d9/libdxvk_d3d9.so)
    ADD_CUSTOM_TARGET(dependencies ALL DEPENDS dxvk-native)
endif()

# Source related part

set(SRC_FILES
    "src/d3d_utility.cpp"
    "src/d3d_utility.h"
    "src/sdl_d3d9_triangle.cpp"
)

add_executable(${PROJECT_NAME} WIN32 ${SRC_FILES})

if (WIN32)
set(SYSTEM_DEPS "CONAN_PKG::directx")
else() # for DXVK
set(SYSTEM_DEPS "${DXVK_NATIVE_D3D9_LIB}")
target_include_directories(${PROJECT_NAME} PRIVATE
    "${DXVK_NATIVE_INCLUDE_DIRS}"
)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${SYSTEM_DEPS}
    CONAN_PKG::sdl
)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
