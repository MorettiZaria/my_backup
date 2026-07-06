cmake_minimum_required(VERSION 3.16)
project(BackupSystem LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 头文件搜索路径
include_directories(${PROJECT_SOURCE_DIR}/include)

# 收集所有源文件
file(GLOB_RECURSE SRC_FILES
    ${PROJECT_SOURCE_DIR}/main.cpp
    ${PROJECT_SOURCE_DIR}/src/*.cpp
)

# 生成可执行文件
add_executable(backup ${SRC_FILES})
