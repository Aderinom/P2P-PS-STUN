cmake_minimum_required(VERSION 3.10)

project(P2P_client VERSION 1.0)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

file(GLOB_RECURSE sorces *.cpp, *.h)

target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC
                           "${PROJECT_BINARY_DIR}"
                           )


add_executable(${CMAKE_PROJECT_NAME} sorces)