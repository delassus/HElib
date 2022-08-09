cmake_minimum_required(VERSION 3.10.2 FATAL_ERROR)

project(MATMULDEMO VERSION 1.0)

set(CMAKE_CXX_FLAGS "-O3 -march=native -Wall -Wextra")

###HELib CMake
## Use -std=c++17 as default.
set(CMAKE_CXX_STANDARD 17)
## Require full C++ standard
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(helib ${HELIB_VERSION} REQUIRED)

set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/build")

add_executable(demo0 "${PROJECT_SOURCE_DIR}/demo0.cpp")
add_executable(demo1 "${PROJECT_SOURCE_DIR}/demo1.cpp")
add_executable(demo2 "${PROJECT_SOURCE_DIR}/demo2.cpp")
add_executable(demo3 "${PROJECT_SOURCE_DIR}/demo3.cpp")
add_executable(demo4 "${PROJECT_SOURCE_DIR}/demo4.cpp")

target_link_libraries(demo0 helib)
target_link_libraries(demo1 helib)
target_link_libraries(demo2 helib)
target_link_libraries(demo3 helib)
target_link_libraries(demo4 helib)
