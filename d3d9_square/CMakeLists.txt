cmake_minimum_required(VERSION 3.5)
project(d3d9_square)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "" FORCE)
endif()

### Set up output paths
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/bin)

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

# Source files

set(SRC_FILES
    "src/d3d9_square.cpp"
)

add_executable(${PROJECT_NAME} ${SRC_FILES})

# Dependencies

if (WIN32)
    message("Using Windows D3D9 API")
    set(NATIVE_D3D9_LIBS "CONAN_PKG::directx")
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    winmm
    ${NATIVE_D3D9_LIBS}
)
