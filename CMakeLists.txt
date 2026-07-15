cmake_minimum_required(VERSION 3.16)
project(BackupSystem VERSION 3.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================
# 公共编译选项
# ============================================================
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ============================================================
# libbackup — 共享核心静态库
# ============================================================
add_subdirectory(libbackup)

# ============================================================
# client — CLI 客户端 (backup 命令)
# ============================================================
add_subdirectory(client)

# ============================================================
# server — 专用服务器守护进程 (backup-server)
# ============================================================
add_subdirectory(server)

# ============================================================
# gui — Qt6 图形界面（可选）
# ============================================================
find_package(Qt6 COMPONENTS Widgets QUIET)
if(Qt6_FOUND)
    message(STATUS "Qt6 found, building GUI target (backup-gui)")
    add_subdirectory(gui)
else()
    message(STATUS "Qt6 not found, skipping GUI target")
endif()

# ============================================================
# GTest 单元测试（可选）
# ============================================================
option(BUILD_TESTS "Build unit tests with GTest" ON)
if(BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.15.2
    )
    FetchContent_MakeAvailable(googletest)
    enable_testing()
    add_subdirectory(tests/gtest)
endif()

# ============================================================
# 安装规则
# ============================================================
include(GNUInstallDirs)

install(TARGETS backup backup-server
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(Qt6_FOUND)
    install(TARGETS backup-gui
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

install(FILES deploy/server.conf.example
        DESTINATION /etc
        RENAME backup-server.conf)

install(FILES deploy/backup-server.service
        DESTINATION /etc/systemd/system)

# ============================================================
# 安装后脚本
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
        message(STATUS "  Start server: backup-server --config /etc/backup-server.conf")
        message(STATUS "  Or install systemd and run: systemctl start backup-server")
    endif()
]])
