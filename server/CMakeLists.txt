# server — 专用服务器守护进程 (backup-server)
# 轻量级服务器二进制，仅包含服务器启动逻辑，适合生产部署

add_executable(backup-server src/main.cpp)
target_link_libraries(backup-server PRIVATE libbackup)
target_compile_options(backup-server PRIVATE -Wall -Wextra -Wpedantic)
