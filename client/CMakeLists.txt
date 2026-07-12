# client — CLI 客户端可执行文件 (backup)
# 提供所有命令行功能：本地备份/还原、远程备份/还原、用户管理、服务器启动

add_executable(backup src/main.cpp)
target_link_libraries(backup PRIVATE libbackup)
target_compile_options(backup PRIVATE -Wall -Wextra -Wpedantic)
