#include "tabs/LocalRestoreTab.h"
#include "workers/RestoreWorker.h"
#include "metadata/MetadataStore.h"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QThread>
#include <QDateTime>
#include <fstream>

LocalRestoreTab::LocalRestoreTab(QWidget* parent)
    : QWidget(parent) {

    auto* mainLayout = new QVBoxLayout(this);
    auto* form = new QFormLayout;

    inputFileEdit_ = new QLineEdit;
    auto* inBrowseBtn = new QPushButton("浏览...");
    auto* inRow = new QHBoxLayout;
    inRow->addWidget(inputFileEdit_);
    inRow->addWidget(inBrowseBtn);
    form->addRow("备份文件:", inRow);
    connect(inBrowseBtn, &QPushButton::clicked, this, &LocalRestoreTab::onBrowseInput);

    destDirEdit_ = new QLineEdit;
    auto* destBrowseBtn = new QPushButton("浏览...");
    auto* destRow = new QHBoxLayout;
    destRow->addWidget(destDirEdit_);
    destRow->addWidget(destBrowseBtn);
    form->addRow("目标目录:", destRow);
    connect(destBrowseBtn, &QPushButton::clicked, this, &LocalRestoreTab::onBrowseDest);

    passwordEdit_ = new QLineEdit;
    passwordEdit_->setEchoMode(QLineEdit::Password);
    passwordEdit_->setPlaceholderText("备份文件加密密码（未加密则留空）");
    form->addRow("文件密码:", passwordEdit_);

    mainLayout->addLayout(form);

    startBtn_ = new QPushButton("▶ 开始还原");
    startBtn_->setStyleSheet("QPushButton { font-size: 14px; padding: 8px 16px; }");
    mainLayout->addWidget(startBtn_);
    connect(startBtn_, &QPushButton::clicked, this, &LocalRestoreTab::onStartRestore);

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

void LocalRestoreTab::onBrowseInput() {
    QString file = QFileDialog::getOpenFileName(this, "选择备份文件", "", "备份文件 (*.bak);;所有文件 (*)");
    if (!file.isEmpty()) inputFileEdit_->setText(file);
}

void LocalRestoreTab::onBrowseDest() {
    QString dir = QFileDialog::getExistingDirectory(this, "选择目标目录");
    if (!dir.isEmpty()) destDirEdit_->setText(dir);
}

void LocalRestoreTab::onStartRestore() {
    QString input = inputFileEdit_->text().trimmed();
    QString dest = destDirEdit_->text().trimmed();

    if (input.isEmpty() || dest.isEmpty()) {
        log("错误：请填写备份文件和目标目录。");
        return;
    }

    // 冲突检测：读取备份文件元数据，检查目标目录中是否有同名文件
    {
        std::ifstream in(input.toStdString(), std::ios::binary);
        if (!in) {
            log("错误：无法打开备份文件。");
            return;
        }
        MetadataStore metaStore;
        BackupHeader hdr = metaStore.loadHeader(in);
        if (hdr.magic != BackupHeader::MAGIC) {
            log("错误：无效的备份文件格式。");
            return;
        }
        std::vector<FileInfo> files = metaStore.loadMetadata(in, hdr.metaSize);

        QStringList conflicts;
        for (const auto& f : files) {
            QString targetPath = dest + "/" + QString::fromStdString(f.relativePath);
            if (QFileInfo::exists(targetPath)) {
                conflicts << QString::fromStdString(f.relativePath);
            }
        }

        if (!conflicts.isEmpty()) {
            QString msg = QString("目标目录中已有以下 %1 个文件/目录存在，还原会覆盖它们：\n\n")
                              .arg(conflicts.size());
            // 最多显示前 10 个冲突
            int showCount = std::min(static_cast<int>(conflicts.size()), 10);
            for (int i = 0; i < showCount; ++i) {
                msg += "  • " + conflicts[i] + "\n";
            }
            if (conflicts.size() > 10) {
                msg += QString("  ... 等共 %1 个\n").arg(conflicts.size());
            }
            msg += "\n请更换目标目录或先清理已有文件后再试。";

            QMessageBox::warning(this, "文件冲突", msg);
            return;
        }
    }

    setFormEnabled(false);
    progressBar_->setVisible(true);
    logView_->clear();
    log(QString("[%1] 开始本地还原...").arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
    log(QString("  备份文件: %1").arg(input));
    log(QString("  目标目录: %1").arg(dest));

    auto* thread = new QThread(this);
    auto* worker = new RestoreWorker(input, dest,
                                     &strategies_.packMgr,
                                     &strategies_.compressMgr,
                                     &strategies_.encryptMgr,
                                     passwordEdit_->text());
    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker, &RestoreWorker::run);
    connect(worker, &RestoreWorker::started, this, &LocalRestoreTab::onWorkerStarted);
    connect(worker, &RestoreWorker::finished, this, &LocalRestoreTab::onWorkerFinished);
    connect(worker, &RestoreWorker::finished, thread, &QThread::quit);
    connect(worker, &RestoreWorker::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);

    thread->start();
}

void LocalRestoreTab::onWorkerStarted() {
    log("正在执行还原操作...");
}

void LocalRestoreTab::onWorkerFinished(bool success, const QString& message) {
    progressBar_->setVisible(false);
    setFormEnabled(true);

    if (success) {
        log(QString("✓ %1").arg(message));
    } else {
        log(QString("✗ %1").arg(message));
    }
}

void LocalRestoreTab::setFormEnabled(bool enabled) {
    inputFileEdit_->setEnabled(enabled);
    destDirEdit_->setEnabled(enabled);
    passwordEdit_->setEnabled(enabled);
    startBtn_->setEnabled(enabled);
}

void LocalRestoreTab::log(const QString& msg) {
    logView_->append(msg);
}
