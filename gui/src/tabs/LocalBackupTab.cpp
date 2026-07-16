#include "tabs/LocalBackupTab.h"
#include "workers/BackupWorker.h"
#include "widgets/FilterSetupWidget.h"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QThread>
#include <QDateTime>
#include <QFileInfo>

LocalBackupTab::LocalBackupTab(QWidget* parent)
    : QWidget(parent) {

    auto* mainLayout = new QVBoxLayout(this);

    // ---- 表单 ----
    auto* form = new QFormLayout;

    sourceDirEdit_ = new QLineEdit;
    auto* srcBrowseBtn = new QPushButton("浏览...");
    auto* srcRow = new QHBoxLayout;
    srcRow->addWidget(sourceDirEdit_);
    srcRow->addWidget(srcBrowseBtn);
    form->addRow("源目录:", srcRow);
    connect(srcBrowseBtn, &QPushButton::clicked, this, &LocalBackupTab::onBrowseSource);

    // 输出目录
    outputDirEdit_ = new QLineEdit;
    outputDirEdit_->setPlaceholderText("备份文件将保存在此目录");
    auto* outDirBtn = new QPushButton("浏览...");
    auto* outDirRow = new QHBoxLayout;
    outDirRow->addWidget(outputDirEdit_);
    outDirRow->addWidget(outDirBtn);
    form->addRow("保存到目录:", outDirRow);
    connect(outDirBtn, &QPushButton::clicked, this, &LocalBackupTab::onBrowseOutputDir);

    // 输出文件名
    outputNameEdit_ = new QLineEdit;
    outputNameEdit_->setPlaceholderText("例如: mybackup.bak");
    form->addRow("文件名:", outputNameEdit_);

    packCombo_ = new QComboBox;
    packCombo_->addItems({"tar", "index"});
    form->addRow("打包方式:", packCombo_);

    compressCombo_ = new QComboBox;
    compressCombo_->addItems({"无", "rle", "huffman"});
    form->addRow("压缩方式:", compressCombo_);

    encryptCombo_ = new QComboBox;
    encryptCombo_->addItems({"无", "xor", "vigenere"});
    form->addRow("加密方式:", encryptCombo_);

    passwordEdit_ = new QLineEdit;
    passwordEdit_->setEchoMode(QLineEdit::Password);
    passwordEdit_->setPlaceholderText("加密密码（选择加密方式时必填）");
    form->addRow("文件密码:", passwordEdit_);

    mainLayout->addLayout(form);

    // ---- 文件筛选 ----
    filterWidget_ = new FilterSetupWidget(this);
    mainLayout->addWidget(filterWidget_);

    // ---- 操作按钮 ----
    startBtn_ = new QPushButton("▶ 开始备份");
    startBtn_->setStyleSheet("QPushButton { font-size: 14px; padding: 8px 16px; }");
    mainLayout->addWidget(startBtn_);
    connect(startBtn_, &QPushButton::clicked, this, &LocalBackupTab::onStartBackup);

    // ---- 进度条 ----
    progressBar_ = new QProgressBar;
    progressBar_->setRange(0, 0);  // indeterminate
    progressBar_->setVisible(false);
    mainLayout->addWidget(progressBar_);

    // ---- 日志区域 ----
    logView_ = new QTextEdit;
    logView_->setReadOnly(true);
    logView_->document()->setMaximumBlockCount(1000);
    logView_->setPlaceholderText("操作日志将显示在这里...");
    mainLayout->addWidget(logView_);

    mainLayout->addStretch();
}

void LocalBackupTab::onBrowseSource() {
    QString dir = QFileDialog::getExistingDirectory(this, "选择源目录");
    if (!dir.isEmpty()) sourceDirEdit_->setText(dir);
}

void LocalBackupTab::onBrowseOutputDir() {
    QString dir = QFileDialog::getExistingDirectory(this, "选择保存目录");
    if (!dir.isEmpty()) outputDirEdit_->setText(dir);
}

void LocalBackupTab::onStartBackup() {
    QString src = sourceDirEdit_->text().trimmed();
    QString outDir = outputDirEdit_->text().trimmed();
    QString outName = outputNameEdit_->text().trimmed();

    if (src.isEmpty()) {
        log("错误：请选择源目录。");
        return;
    }
    if (outDir.isEmpty()) {
        log("错误：请选择保存到目录。");
        return;
    }
    if (outName.isEmpty()) {
        log("错误：请填写文件名。");
        return;
    }

    // 自动补 .bak 后缀
    if (!outName.endsWith(".bak", Qt::CaseInsensitive)) {
        outName += ".bak";
    }

    // 构造完整输出路径
    QString out = outDir + "/" + outName;

    // 冲突检测：输出文件已存在则阻止
    if (QFileInfo::exists(out)) {
        QMessageBox::warning(this, "文件已存在",
            QString("输出文件已存在，请更换文件名或目录：\n\n%1").arg(out));
        return;
    }

    QString encryptName = encryptCombo_->currentText();
    QString password = passwordEdit_->text();

    if (encryptName != "无" && password.isEmpty()) {
        log("错误：选择加密方式时必须填写文件密码。");
        return;
    }

    // 选择策略
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

    // UI 状态
    setFormEnabled(false);
    progressBar_->setVisible(true);
    logView_->clear();
    log(QString("[%1] 开始本地备份...").arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
    log(QString("  源目录: %1").arg(src));
    log(QString("  输出文件: %1").arg(out));
    log(QString("  打包: %1  压缩: %2  加密: %3").arg(packName, compressName, encryptName));

    // 创建线程 + Worker
    auto* thread = new QThread(this);
    auto* worker = new BackupWorker(src, out, pack, compress, encrypt, password,
                                    filterWidget_->buildFilter());
    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker, &BackupWorker::run);
    connect(worker, &BackupWorker::started, this, &LocalBackupTab::onWorkerStarted);
    connect(worker, &BackupWorker::finished, this, &LocalBackupTab::onWorkerFinished);
    connect(worker, &BackupWorker::finished, thread, &QThread::quit);
    connect(worker, &BackupWorker::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);

    thread->start();
}

void LocalBackupTab::onWorkerStarted() {
    log("正在执行备份操作...");
}

void LocalBackupTab::onWorkerFinished(bool success, const QString& message) {
    progressBar_->setVisible(false);
    setFormEnabled(true);

    if (success) {
        log(QString("✓ %1").arg(message));
    } else {
        log(QString("✗ %1").arg(message));
    }
}

void LocalBackupTab::setFormEnabled(bool enabled) {
    sourceDirEdit_->setEnabled(enabled);
    outputDirEdit_->setEnabled(enabled);
    outputNameEdit_->setEnabled(enabled);
    packCombo_->setEnabled(enabled);
    compressCombo_->setEnabled(enabled);
    encryptCombo_->setEnabled(enabled);
    passwordEdit_->setEnabled(enabled);
    filterWidget_->setFormEnabled(enabled);
    startBtn_->setEnabled(enabled);
}

void LocalBackupTab::log(const QString& msg) {
    logView_->append(msg);
}
