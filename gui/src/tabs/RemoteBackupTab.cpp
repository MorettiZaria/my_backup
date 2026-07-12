#include "tabs/RemoteBackupTab.h"
#include "workers/RemoteBackupWorker.h"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QThread>
#include <QDateTime>
#include <QIntValidator>

RemoteBackupTab::RemoteBackupTab(QWidget* parent)
    : QWidget(parent) {

    auto* mainLayout = new QVBoxLayout(this);
    auto* form = new QFormLayout;

    // 服务器配置
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

    // 备份配置
    sourceDirEdit_ = new QLineEdit;
    auto* srcBrowseBtn = new QPushButton("浏览...");
    auto* srcRow = new QHBoxLayout;
    srcRow->addWidget(sourceDirEdit_);
    srcRow->addWidget(srcBrowseBtn);
    form->addRow("源目录:", srcRow);
    connect(srcBrowseBtn, &QPushButton::clicked, this, &RemoteBackupTab::onBrowseSource);

    packCombo_ = new QComboBox;
    packCombo_->addItems({"tar", "index"});
    form->addRow("打包方式:", packCombo_);

    compressCombo_ = new QComboBox;
    compressCombo_->addItems({"无", "rle", "huffman"});
    form->addRow("压缩方式:", compressCombo_);

    encryptCombo_ = new QComboBox;
    encryptCombo_->addItems({"无", "xor", "vigenere"});
    form->addRow("加密方式:", encryptCombo_);

    filePasswordEdit_ = new QLineEdit;
    filePasswordEdit_->setEchoMode(QLineEdit::Password);
    filePasswordEdit_->setPlaceholderText("文件加密密码（选择加密方式时必填）");
    form->addRow("文件密码:", filePasswordEdit_);

    mainLayout->addLayout(form);

    startBtn_ = new QPushButton("▶ 开始远程备份");
    startBtn_->setStyleSheet("QPushButton { font-size: 14px; padding: 8px 16px; }");
    mainLayout->addWidget(startBtn_);
    connect(startBtn_, &QPushButton::clicked, this, &RemoteBackupTab::onStartBackup);

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

void RemoteBackupTab::onBrowseSource() {
    QString dir = QFileDialog::getExistingDirectory(this, "选择源目录");
    if (!dir.isEmpty()) sourceDirEdit_->setText(dir);
}

void RemoteBackupTab::onStartBackup() {
    QString server = serverEdit_->text().trimmed();
    QString username = usernameEdit_->text().trimmed();
    QString password = passwordEdit_->text();
    QString src = sourceDirEdit_->text().trimmed();

    if (server.isEmpty() || username.isEmpty() || password.isEmpty() || src.isEmpty()) {
        log("错误：请填写服务器地址、用户名、登录密码和源目录。");
        return;
    }

    QString encryptName = encryptCombo_->currentText();
    QString filePassword = filePasswordEdit_->text();

    if (encryptName != "无" && filePassword.isEmpty()) {
        log("错误：选择加密方式时必须填写文件密码。");
        return;
    }

    QString packName = packCombo_->currentText();
    QString compressName = compressCombo_->currentText();

    auto* pack = strategies_.packMgr.select(packName.toStdString());
    ICompressStrategy* compress = nullptr;
    if (compressName != "无") {
        compress = strategies_.compressMgr.select(compressName.toStdString());
    }
    IEncryptStrategy* encrypt = nullptr;
    if (encryptName != "无") {
        encrypt = strategies_.encryptMgr.select(encryptName.toStdString());
    }

    setFormEnabled(false);
    progressBar_->setVisible(true);
    logView_->clear();
    log(QString("[%1] 开始远程备份...").arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
    log(QString("  服务器: %1:%2").arg(server).arg(portSpin_->value()));
    log(QString("  用户: %1  源目录: %2").arg(username, src));
    log(QString("  打包: %1  压缩: %2  加密: %3").arg(packName, compressName, encryptName));

    auto* thread = new QThread(this);
    auto* worker = new RemoteBackupWorker(server,
                                          static_cast<uint16_t>(portSpin_->value()),
                                          username, password, src,
                                          pack, compress, encrypt, filePassword);
    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker, &RemoteBackupWorker::run);
    connect(worker, &RemoteBackupWorker::started, this, &RemoteBackupTab::onWorkerStarted);
    connect(worker, &RemoteBackupWorker::finished, this, &RemoteBackupTab::onWorkerFinished);
    connect(worker, &RemoteBackupWorker::finished, thread, &QThread::quit);
    connect(worker, &RemoteBackupWorker::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);

    thread->start();
}

void RemoteBackupTab::onWorkerStarted() {
    log("正在连接服务器并执行远程备份...");
}

void RemoteBackupTab::onWorkerFinished(bool success, const QString& message) {
    progressBar_->setVisible(false);
    setFormEnabled(true);

    if (success) {
        log(QString("✓ %1").arg(message));
    } else {
        log(QString("✗ %1").arg(message));
    }
}

void RemoteBackupTab::setFormEnabled(bool enabled) {
    serverEdit_->setEnabled(enabled);
    portSpin_->setEnabled(enabled);
    usernameEdit_->setEnabled(enabled);
    passwordEdit_->setEnabled(enabled);
    sourceDirEdit_->setEnabled(enabled);
    packCombo_->setEnabled(enabled);
    compressCombo_->setEnabled(enabled);
    encryptCombo_->setEnabled(enabled);
    filePasswordEdit_->setEnabled(enabled);
    startBtn_->setEnabled(enabled);
}

void RemoteBackupTab::log(const QString& msg) {
    logView_->append(msg);
}
