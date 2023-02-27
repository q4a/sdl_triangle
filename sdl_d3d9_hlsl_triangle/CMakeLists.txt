cmake_minimum_required(VERSION 3.18)
project(sdl_d3d9_hlsl_triangle)

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

if (WIN32 OR USE_CONAN)
include(conan)
### for CONAN_PKG::directx
conan_add_remote(NAME storm
    URL https://storm.jfrog.io/artifactory/api/conan/conan-remote
    VERIFY_SSL True
)
conan_cmake_run(CONANFILE conanfile.py
    BASIC_SETUP CMAKE_TARGETS
    BUILD missing
)
set(SDL_DEPS "CONAN_PKG::sdl")
else()
#sudo apt install libsdl2-dev
set(SDL_DEPS "SDL2")
endif()

if (WIN32)
message("Using Windows D3D9 API")
else()

include(ProcessorCount)
ProcessorCount(N)
message("ProcessorCount = ${N}")

set(VKD3D_CONFIG "./configure")
list(APPEND VKD3D_CONFIG "CPPFLAGS=-O3 -DNDEBUG -DVKD3D_NO_TRACE_MESSAGES")
list(APPEND VKD3D_CONFIG   "CFLAGS=-O3 -DNDEBUG -DVKD3D_NO_TRACE_MESSAGES")

include(ExternalProject)
ExternalProject_Add(vkd3d
    GIT_REPOSITORY    https://gitlab.winehq.org/wine/vkd3d.git
    GIT_TAG           b1e13d6e33dd2be6a4b127d6c75fd088c5d6edee
    GIT_SHALLOW       ON
    BUILD_ALWAYS      OFF
    BUILD_IN_SOURCE   ON
    CONFIGURE_HANDLED_BY_BUILD ON
    # result comman will be: ./configure "CPPFLAGS=-O3 -DNDEBUG -DVKD3D_NO_TRACE_MESSAGES" "CFLAGS=-O3 -DNDEBUG -DVKD3D_NO_TRACE_MESSAGES"
    CONFIGURE_COMMAND ./autogen.sh && ${CMAKE_COMMAND} -E env ${VKD3D_CONFIG}
    BUILD_COMMAND     make -j${N}
    INSTALL_COMMAND   ""
)
ExternalProject_Get_property(vkd3d SOURCE_DIR BINARY_DIR)
set(VKD3D_INCLUDE_DIRS
    "${SOURCE_DIR}/include"
)
set(VKD3D_LIB ${BINARY_DIR}/.libs/libvkd3d-utils.so)

if (USE_NINE) # for Gallium Nine
    message("Using Gallium Nine for native D3D9 API")

    #sudo apt install libd3dadapter9-mesa-dev
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(D3D REQUIRED IMPORTED_TARGET d3d)

    add_subdirectory(../nine-native nine-native)
    set(NINE_NATIVE_INCLUDE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}/../nine-native/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../nine-native/include/D3D9"
    )
    ADD_CUSTOM_TARGET(dependencies ALL DEPENDS vkd3d)
else() # for DXVK Native
    message("Using DXVK Native for D3D9 API")

    ExternalProject_Add(dxvk-native
        GIT_REPOSITORY    https://github.com/q4a/dxvk-native
        GIT_TAG           44c054faf76ce90578dee79a955e8aefa7356368
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
    ADD_CUSTOM_TARGET(dependencies ALL DEPENDS vkd3d dxvk-native)
endif()
endif() # WIN32

# Source related part

if (USE_NINE)
    add_definitions(-DUSE_NINE=1)
endif()

set(SRC_FILES
    "src/d3d_utility.cpp"
    "src/d3d_utility.h"
    "src/sdl_d3d9_hlsl_triangle.cpp"
)

if (MSVC)
    add_compile_options(/std:c++latest) # for filesystem
else()
    add_compile_options(-std=c++20) # for filesystem
endif()

add_executable(${PROJECT_NAME} WIN32 ${SRC_FILES})

if (WIN32)
    set(SYSTEM_DEPS "CONAN_PKG::directx")
elseif (USE_NINE) # for Gallium Nine
    set(SYSTEM_DEPS
        "${VKD3D_LIB}"
        nine-native
        "PkgConfig::D3D"
    )
    target_include_directories(${PROJECT_NAME} PRIVATE
        "${VKD3D_INCLUDE_DIRS}"
        "${NINE_NATIVE_INCLUDE_DIRS}"
    )
else() # for DXVK Native
    set(SYSTEM_DEPS
        "${VKD3D_LIB}"
        "${DXVK_NATIVE_D3D9_LIB}"
    )
    target_include_directories(${PROJECT_NAME} PRIVATE
        "${VKD3D_INCLUDE_DIRS}"
        "${DXVK_NATIVE_INCLUDE_DIRS}"
    )
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${SYSTEM_DEPS}
    ${SDL_DEPS}
)

macro(configure_files srcDir destDir)
    message(STATUS "Configuring directory ${destDir}")
    make_directory(${destDir})

    file(GLOB_RECURSE templateFiles RELATIVE ${srcDir} "${srcDir}/*")
    foreach(templateFile ${templateFiles})
        set(srcTemplatePath ${srcDir}/${templateFile})
        if(NOT IS_DIRECTORY ${srcTemplatePath})
            message(STATUS "Configuring file ${templateFile}")
            configure_file(
                    ${srcTemplatePath}
                    ${destDir}/${templateFile}
                    COPYONLY)
        endif(NOT IS_DIRECTORY ${srcTemplatePath})
    endforeach(templateFile)
endmacro(configure_files)
configure_files(${PROJECT_SOURCE_DIR}/shaders ${PROJECT_BINARY_DIR}/bin/shaders)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
