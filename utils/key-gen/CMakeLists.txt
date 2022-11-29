# Copyright (C) 2020 IBM Corp.
# This program is Licensed under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance
# with the License. You may obtain a copy of the License at
#   http://www.apache.org/licenses/LICENSE-2.0
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License. See accompanying LICENSE file.
cmake_minimum_required(VERSION 3.16)
project(Keygen)
set(CMAKE_CXX_STANDARD 17)
set(helib_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../helib_install/helib_pack/share/cmake/helib")
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../../VERSION" HELIB_VERSION)
find_package(helib "${HELIB_VERSION}" EXACT REQUIRED)
add_executable(key-gen key-gen.cpp)
add_executable(gen-data gen-data.cpp)

target_include_directories(gen-data PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(key-gen PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(key-gen PRIVATE "../common")
target_include_directories(gen-data PRIVATE "../common")

target_link_libraries(key-gen helib)
target_link_libraries(gen-data helib)
