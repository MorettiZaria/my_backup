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

# ============================================================
# 安装规则
# ============================================================
include(GNUInstallDirs)

# CLI 二进制
install(TARGETS backup RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Qt6 GUI 二进制（如果编译了）
if(Qt6_FOUND)
    install(TARGETS backup-gui RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# 配置文件（安装示例配置，已存在则跳过）
install(FILES deploy/server.conf.example
        DESTINATION /etc
        RENAME backup-server.conf)

# systemd 服务单元
install(FILES deploy/backup-server.service
        DESTINATION /etc/systemd/system)

# ============================================================
# 安装后脚本：创建用户、目录、启用服务
# ============================================================
install(CODE [[
    # ---- 创建 backup 系统用户 ----
    execute_process(
        COMMAND id -u backup
        RESULT_VARIABLE USER_EXISTS
        OUTPUT_QUIET ERROR_QUIET
    )
    if(NOT USER_EXISTS EQUAL 0)
        message(STATUS "Creating system user 'backup'...")
        execute_process(
            COMMAND useradd -r -s /usr/sbin/nologin -m -d /var/lib/backup-server backup
            RESULT_VARIABLE UA_RESULT
        )
        if(NOT UA_RESULT EQUAL 0)
            message(WARNING "Failed to create 'backup' user. You may need to create it manually.")
        endif()
    endif()

    # ---- 创建数据和日志目录 ----
    message(STATUS "Creating data directories...")
    file(MAKE_DIRECTORY /var/lib/backup-server/data)
    execute_process(COMMAND touch /var/log/backup-server.log
                    RESULT_VARIABLE TOUCH_RESULT)
    if(NOT TOUCH_RESULT EQUAL 0)
        message(WARNING "Cannot create /var/log/backup-server.log")
    endif()

    # ---- 设置权限 ----
    execute_process(
        COMMAND chown -R backup:backup /var/lib/backup-server
        RESULT_VARIABLE CHOWN1
    )
    execute_process(
        COMMAND chown backup:backup /var/log/backup-server.log
        RESULT_VARIABLE CHOWN2
    )
    execute_process(
        COMMAND chmod 750 /var/lib/backup-server
        RESULT_VARIABLE CHMOD
    )

    # ---- 重载 systemd ----
    if(EXISTS /run/systemd/system)
        message(STATUS "Reloading systemd...")
        execute_process(COMMAND systemctl daemon-reload)
        execute_process(COMMAND systemctl enable backup-server
                        RESULT_VARIABLE ENABLE_RESULT)
        if(NOT ENABLE_RESULT EQUAL 0)
            message(WARNING "Failed to enable backup-server service.")
        endif()
        message(STATUS "")
        message(STATUS "=== Installation complete! ===")
        message(STATUS "  Start server: systemctl start backup-server")
        message(STATUS "  Check status: systemctl status backup-server")
        message(STATUS "  View logs:    journalctl -u backup-server -f")
    else()
        message(STATUS "")
        message(STATUS "=== Installation complete! ===")
        message(STATUS "  Start server: backup server start --config /etc/backup-server.conf")
        message(STATUS "  Or install systemd and run: systemctl start backup-server")
    endif()
]])
