#include "MainWindow.h"
#include "tabs/LocalBackupTab.h"
#include "tabs/LocalRestoreTab.h"
#include "tabs/RemoteBackupTab.h"
#include "tabs/RemoteRestoreTab.h"
#include "tabs/RemoteListTab.h"
#include "tabs/UserManagementTab.h"

#include <QTabWidget>
#include <QStatusBar>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {

    setWindowTitle("my_backup - 数据备份与还原系统");
    resize(680, 580);

    tabWidget_ = new QTabWidget;
    tabWidget_->addTab(new LocalBackupTab,    "📦 本地备份");
    tabWidget_->addTab(new LocalRestoreTab,   "📂 本地还原");
    tabWidget_->addTab(new RemoteBackupTab,   "☁ 远程备份");
    tabWidget_->addTab(new RemoteRestoreTab,  "📥 远程还原");
    tabWidget_->addTab(new RemoteListTab,     "📋 远程列表");
    tabWidget_->addTab(new UserManagementTab, "👤 用户管理");

    setCentralWidget(tabWidget_);

    statusBar()->showMessage("就绪 — 请选择操作选项卡");
}
