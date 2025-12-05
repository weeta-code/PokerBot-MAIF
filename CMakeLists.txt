cmake_minimum_required(VERSION 3.15)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(PokerBotMAIF VERSION 1.0 LANGUAGES CXX)

include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/include/mccfr)

# src files
set(SOURCES
    src/main.cpp
    src/equity.cpp
    src/risk_profiler.cpp
    src/game_state.cpp
    src/mccfr/trainer.cpp
    src/mccfr/node.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -pedantic)

# output directory 
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build/bin
)