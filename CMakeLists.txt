cmake_minimum_required(VERSION 3.15)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(PokerBot)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# header files
include_directories(${PROJECT_SOURCE_DIR}/include)

# src files
file(GLOB_RECURSE SOURCES "src/*.cpp")

# executable
add_executable(${PROJECT_NAME} ${SOURCES})

# include warnings
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -pedantic)

# output directory
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build/bin
)