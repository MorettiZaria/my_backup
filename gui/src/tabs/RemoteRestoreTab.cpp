#include "tabs/RemoteRestoreTab.h"
#include "workers/RemoteRestoreWorker.h"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QThread>
#include <QDateTime>

RemoteRestoreTab::RemoteRestoreTab(QWidget* parent)
    : QWidget(parent) {

    auto* mainLayout = new QVBoxLayout(this);
    auto* form = new QFormLayout;

    serverEdit_ = new QLineEdit;
    serverEdit_->setPlaceholderText("例如: 192.168.1.100");
    form->addRow("服务器地址:", serverEdit_);

    portSpin_ = new QSpinBox;
    portSpin_->setRange(1, 65535);
    portSpin_->setValue(8848);
    form->addRow("端口:", portSpin_);

    usernameEdit_ = new QLineEdit;
    form->addRow("用户名:", usernameEdit_);

    passwordEdit_ = new QLineEdit;
    passwordEdit_->setEchoMode(QLineEdit::Password);
    form->addRow("登录密码:", passwordEdit_);

    destDirEdit_ = new QLineEdit;
    auto* destBrowseBtn = new QPushButton("浏览...");
    auto* destRow = new QHBoxLayout;
    destRow->addWidget(destDirEdit_);
    destRow->addWidget(destBrowseBtn);
    form->addRow("目标目录:", destRow);
    connect(destBrowseBtn, &QPushButton::clicked, this, &RemoteRestoreTab::onBrowseDest);

    backupIdEdit_ = new QLineEdit;
    backupIdEdit_->setPlaceholderText("留空则还原最新备份");
    form->addRow("备份 ID:", backupIdEdit_);

    filePasswordEdit_ = new QLineEdit;
    filePasswordEdit_->setEchoMode(QLineEdit::Password);
    filePasswordEdit_->setPlaceholderText("备份文件加密密码（未加密则留空）");
    form->addRow("文件密码:", filePasswordEdit_);

    mainLayout->addLayout(form);

    startBtn_ = new QPushButton("▶ 开始远程还原");
    startBtn_->setStyleSheet("QPushButton { font-size: 14px; padding: 8px 16px; }");
    mainLayout->addWidget(startBtn_);
    connect(startBtn_, &QPushButton::clicked, this, &RemoteRestoreTab::onStartRestore);

    progressBar_ = new QProgressBar;
    progressBar_->setRange(0, 0);
    progressBar_->setVisible(false);
    mainLayout->addWidget(progressBar_);

    logView_ = new QTextEdit;
    logView_->setReadOnly(true);
    logView_->document()->setMaximumBlockCount(1000);
    logView_->setPlaceholderText("操作日志将显示在这里...");
    mainLayout->addWidget(logView_);

    mainLayout->addStretch();
}

void RemoteRestoreTab::onBrowseDest() {
    QString dir = QFileDialog::getExistingDirectory(this, "选择目标目录");
    if (!dir.isEmpty()) destDirEdit_->setText(dir);
}

void RemoteRestoreTab::onStartRestore() {
    QString server = serverEdit_->text().trimmed();
    QString username = usernameEdit_->text().trimmed();
    QString password = passwordEdit_->text();
    QString dest = destDirEdit_->text().trimmed();

    if (server.isEmpty() || username.isEmpty() || password.isEmpty() || dest.isEmpty()) {
        log("错误：请填写服务器地址、用户名、登录密码和目标目录。");
        return;
    }

    setFormEnabled(false);
    progressBar_->setVisible(true);
    logView_->clear();
    log(QString("[%1] 开始远程还原...").arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
    log(QString("  服务器: %1:%2").arg(server).arg(portSpin_->value()));
    log(QString("  用户: %1  目标目录: %2").arg(username, dest));
    if (!backupIdEdit_->text().isEmpty()) {
        log(QString("  备份 ID: %1").arg(backupIdEdit_->text()));
    } else {
        log("  备份 ID: (最新)");
    }

    auto* thread = new QThread(this);
    auto* worker = new RemoteRestoreWorker(server,
                                           static_cast<uint16_t>(portSpin_->value()),
                                           username, password, dest,
                                           backupIdEdit_->text().trimmed(),
                                           filePasswordEdit_->text());
    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker, &RemoteRestoreWorker::run);
    connect(worker, &RemoteRestoreWorker::started, this, &RemoteRestoreTab::onWorkerStarted);
    connect(worker, &RemoteRestoreWorker::finished, this, &RemoteRestoreTab::onWorkerFinished);
    connect(worker, &RemoteRestoreWorker::finished, thread, &QThread::quit);
    connect(worker, &RemoteRestoreWorker::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);

    thread->start();
}

void RemoteRestoreTab::onWorkerStarted() {
    log("正在连接服务器并执行远程还原...");
}

void RemoteRestoreTab::onWorkerFinished(bool success, const QString& message) {
    progressBar_->setVisible(false);
    setFormEnabled(true);

    if (success) {
        log(QString("✓ %1").arg(message));
    } else {
        log(QString("✗ %1").arg(message));
    }
}

void RemoteRestoreTab::setFormEnabled(bool enabled) {
    serverEdit_->setEnabled(enabled);
    portSpin_->setEnabled(enabled);
    usernameEdit_->setEnabled(enabled);
    passwordEdit_->setEnabled(enabled);
    destDirEdit_->setEnabled(enabled);
    backupIdEdit_->setEnabled(enabled);
    filePasswordEdit_->setEnabled(enabled);
    startBtn_->setEnabled(enabled);
}

void RemoteRestoreTab::log(const QString& msg) {
    logView_->append(msg);
}
