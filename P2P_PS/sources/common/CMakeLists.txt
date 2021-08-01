cmake_minimum_required(VERSION 3.10)

project(common CXX)

file(GLOB_RECURSE source *.c *.cpp *.h *.hpp )

add_library(${PROJECT_NAME} ${source})


