cmake_minimum_required(VERSION 3.16)
project(BackupSystem LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 头文件搜索路径
include_directories(${PROJECT_SOURCE_DIR}/include)

# 收集所有源文件（分开 glob 避免 recursive 匹配到 gui/main.cpp）
file(GLOB SRC_FILES
    ${PROJECT_SOURCE_DIR}/main.cpp
)
file(GLOB_RECURSE SRC_SUB
    ${PROJECT_SOURCE_DIR}/src/*.cpp
)
list(APPEND SRC_FILES ${SRC_SUB})

# 生成可执行文件
add_executable(backup ${SRC_FILES})

# ============================================================
# Qt6 GUI 目标（可选，不影响 CLI 构建）
# ============================================================
find_package(Qt6 COMPONENTS Widgets QUIET)
if(Qt6_FOUND)
    message(STATUS "Qt6 found, building GUI target (backup-gui)")

    # GUI 源文件（独立的 gui/ 目录）
    file(GLOB_RECURSE GUI_SRC_FILES
        ${PROJECT_SOURCE_DIR}/gui/*.cpp
    )
    # 核心库源文件（不含 CLI 的 main.cpp）
    file(GLOB_RECURSE LIB_SRC_FILES
        ${PROJECT_SOURCE_DIR}/src/*.cpp
    )

    add_executable(backup-gui ${GUI_SRC_FILES} ${LIB_SRC_FILES})
    target_include_directories(backup-gui PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/gui
    )
    target_link_libraries(backup-gui PRIVATE Qt6::Widgets)

    # 启用 Qt 的 AUTOMOC 以处理 Q_OBJECT 宏
    set_target_properties(backup-gui PROPERTIES
        AUTOMOC ON
        CMAKE_CXX_STANDARD 17
        CMAKE_CXX_STANDARD_REQUIRED ON
    )
else()
    message(STATUS "Qt6 not found, skipping GUI target (CLI target still available)")
endif()
