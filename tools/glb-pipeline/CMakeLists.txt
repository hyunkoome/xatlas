cmake_minimum_required(VERSION 3.21)
project(glb_pipeline LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

# --- xatlas: compile the fork's single-file source directly -------------------
set(XATLAS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../source/xatlas)
add_library(xatlas STATIC ${XATLAS_DIR}/xatlas.cpp)
target_include_directories(xatlas PUBLIC ${XATLAS_DIR})
find_package(Threads REQUIRED)
target_link_libraries(xatlas PUBLIC Threads::Threads)

# --- third-party from vcpkg ---------------------------------------------------
find_package(meshoptimizer CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_path(TINYGLTF_INCLUDE_DIRS "tiny_gltf.h" REQUIRED)
find_path(STB_INCLUDE_DIRS "stb_image.h" REQUIRED)

# --- executable ---------------------------------------------------------------
add_executable(glb-pipeline
  src/main.cpp
  src/tiny_impl.cpp
)
target_include_directories(glb-pipeline PRIVATE
  ${TINYGLTF_INCLUDE_DIRS}
  ${STB_INCLUDE_DIRS}
  src
)
target_link_libraries(glb-pipeline PRIVATE
  xatlas
  meshoptimizer::meshoptimizer
  nlohmann_json::nlohmann_json
)

install(TARGETS glb-pipeline RUNTIME DESTINATION bin)
