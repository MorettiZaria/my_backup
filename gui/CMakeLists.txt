# gui — Qt6 图形界面客户端 (backup-gui)

set(GUI_HEADERS
    include/MainWindow.h
    include/StrategySetup.h
    include/tabs/LocalBackupTab.h
    include/tabs/LocalRestoreTab.h
    include/tabs/RemoteBackupTab.h
    include/tabs/RemoteRestoreTab.h
    include/tabs/RemoteListTab.h
    include/tabs/UserManagementTab.h
    include/workers/BackupWorker.h
    include/workers/RestoreWorker.h
    include/workers/RemoteBackupWorker.h
    include/workers/RemoteRestoreWorker.h
    include/workers/RemoteListWorker.h
    include/workers/RegisterWorker.h
    include/widgets/FilterSetupWidget.h
)

set(GUI_SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/StrategySetup.cpp
    src/tabs/LocalBackupTab.cpp
    src/tabs/LocalRestoreTab.cpp
    src/tabs/RemoteBackupTab.cpp
    src/tabs/RemoteRestoreTab.cpp
    src/tabs/RemoteListTab.cpp
    src/tabs/UserManagementTab.cpp
    src/workers/BackupWorker.cpp
    src/workers/RestoreWorker.cpp
    src/workers/RemoteBackupWorker.cpp
    src/workers/RemoteRestoreWorker.cpp
    src/workers/RemoteListWorker.cpp
    src/workers/RegisterWorker.cpp
    src/widgets/FilterSetupWidget.cpp
)

add_executable(backup-gui ${GUI_SOURCES} ${GUI_HEADERS})

target_include_directories(backup-gui PRIVATE
    include
    ${CMAKE_SOURCE_DIR}/libbackup/include
)

target_link_libraries(backup-gui PRIVATE Qt6::Widgets libbackup)
target_compile_options(backup-gui PRIVATE -Wall -Wextra -Wpedantic)

# 启用 Qt MOC 以处理 Q_OBJECT 宏
set_target_properties(backup-gui PROPERTIES
    AUTOMOC ON
)
