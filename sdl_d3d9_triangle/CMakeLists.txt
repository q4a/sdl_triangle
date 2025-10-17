cmake_minimum_required(VERSION 3.18)
project(sdl_d3d9_triangle)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")

if (NOT WIN32)
option(USE_NINE "Use Gallium Nine for native D3D9 API" OFF)
endif()

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "" FORCE)
endif()

### Set up output paths
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${PROJECT_BINARY_DIR}/${CMAKE_BUILD_TYPE})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${PROJECT_BINARY_DIR}/${CMAKE_BUILD_TYPE})

find_package(SDL2 CONFIG REQUIRED)

# Source files

if (USE_NINE)
    add_definitions(-DUSE_NINE=1)
endif()

set(SRC_FILES
    "src/d3d_utility.cpp"
    "src/d3d_utility.h"
    "src/sdl_d3d9_triangle.cpp"
)

add_executable(${PROJECT_NAME} WIN32 ${SRC_FILES})

# Dependencies

if (WIN32)
    message("Using Windows D3D9 API")
    set(DIRECTX_INCLUDE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}/../external/dxsdk-june2010/Include"
    )
    set(NATIVE_D3D9_LIBS
        "${CMAKE_CURRENT_SOURCE_DIR}/../external/dxsdk-june2010/Lib/x64/d3d9.lib"
        "${CMAKE_CURRENT_SOURCE_DIR}/../external/dxsdk-june2010/Lib/x64/d3dx9.lib"
    )
    target_include_directories(${PROJECT_NAME} PRIVATE
        "${DIRECTX_INCLUDE_DIRS}"
    )
elseif (USE_NINE) # for Gallium Nine
    message("Using Gallium Nine for native D3D9 API")

    #sudo apt install libd3dadapter9-mesa-dev
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(D3D REQUIRED IMPORTED_TARGET d3d)

    include(ExternalProject)
    ExternalProject_Add(nine-native
        GIT_REPOSITORY    https://github.com/q4a/nine-native
        GIT_TAG           main
        GIT_SHALLOW       ON
        BUILD_ALWAYS      OFF
        BUILD_BYPRODUCTS  <BINARY_DIR>/libnine-native.a
        INSTALL_COMMAND   ""
    )
    ExternalProject_Get_property(nine-native SOURCE_DIR BINARY_DIR)
    set(NINE_NATIVE_INCLUDE_DIRS
        "${SOURCE_DIR}/include"
        "${SOURCE_DIR}/include/D3D9"
        "${SOURCE_DIR}/include/windows"
    )
    set(NATIVE_D3D9_LIBS
        "${BINARY_DIR}/libnine-native.a"
        "PkgConfig::D3D"
        X11
        xcb
        xcb-present
        xcb-dri3
        xcb-xfixes
        X11-xcb
    )
    target_include_directories(${PROJECT_NAME} PRIVATE
        "${NINE_NATIVE_INCLUDE_DIRS}"
    )
    add_dependencies(${PROJECT_NAME} nine-native)
else() # for DXVK Native
    message("Using DXVK Native for D3D9 API")

    include(ExternalProject)
    ExternalProject_Add(dxvk-native
        GIT_REPOSITORY    https://github.com/q4a/dxvk-native
        GIT_TAG           master
        GIT_SHALLOW       ON
        BUILD_ALWAYS      OFF
        CONFIGURE_HANDLED_BY_BUILD ON
        CONFIGURE_COMMAND meson setup ../dxvk-native --buildtype=release -Denable_d3d11=false -Denable_d3d10=false -Denable_dxgi=false
        BUILD_COMMAND     ninja
        BUILD_BYPRODUCTS  <BINARY_DIR>/src/d3d9/libdxvk_d3d9.so
        INSTALL_COMMAND   ""
    )
    ExternalProject_Get_property(dxvk-native SOURCE_DIR BINARY_DIR)
    set(DXVK_NATIVE_INCLUDE_DIRS
        "${SOURCE_DIR}/include/native/directx"
        "${SOURCE_DIR}/include/native/windows"
    )
    set(NATIVE_D3D9_LIBS ${BINARY_DIR}/src/d3d9/libdxvk_d3d9.so)
    target_include_directories(${PROJECT_NAME} PRIVATE
        "${DXVK_NATIVE_INCLUDE_DIRS}"
    )
    add_dependencies(${PROJECT_NAME} dxvk-native)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${NATIVE_D3D9_LIBS}
    SDL2::SDL2
    SDL2::SDL2main
)
