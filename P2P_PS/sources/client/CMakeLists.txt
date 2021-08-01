cmake_minimum_required(VERSION 3.10)

project(p2p_client CXX)

file(GLOB_RECURSE sources	
	${PROJECT_SOURCE_DIR}/src/*.c
	${PROJECT_SOURCE_DIR}/src/*.cpp
	${PROJECT_SOURCE_DIR}/src/*.h
	${PROJECT_SOURCE_DIR}/src/*.hpp
)

add_executable(${PROJECT_NAME} ${sources})

target_link_libraries(${PROJECT_NAME} common)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/sources)
