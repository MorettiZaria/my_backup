#include "tabs/RemoteListTab.h"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QThread>
#include <QDateTime>

RemoteListTab::RemoteListTab(QWidget* parent)
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

    mainLayout->addLayout(form);

    // 刷新按钮
    refreshBtn_ = new QPushButton("⟳ 刷新列表");
    refreshBtn_->setStyleSheet("QPushButton { font-size: 14px; padding: 8px 16px; }");
    mainLayout->addWidget(refreshBtn_);
    connect(refreshBtn_, &QPushButton::clicked, this, &RemoteListTab::onRefresh);

    // 进度条
    progressBar_ = new QProgressBar;
    progressBar_->setRange(0, 0);
    progressBar_->setVisible(false);
    mainLayout->addWidget(progressBar_);

    // 备份列表表格
    table_ = new QTableWidget(0, 3);
    table_->setHorizontalHeaderLabels({"备份名称", "备份 ID", "时间戳"});
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setAlternatingRowColors(true);
    // QTableWidget 没有 setPlaceholderText，通过 tooltip 提示
    table_->setToolTip("请先填写服务器信息，然后点击\"刷新列表\"");
    mainLayout->addWidget(table_);

    // 日志区域
    logView_ = new QTextEdit;
    logView_->setReadOnly(true);
    logView_->document()->setMaximumBlockCount(500);
    logView_->setMaximumHeight(100);
    logView_->setPlaceholderText("操作日志...");
    mainLayout->addWidget(logView_);

    mainLayout->addStretch();
}

void RemoteListTab::onRefresh() {
    QString server = serverEdit_->text().trimmed();
    QString username = usernameEdit_->text().trimmed();
    QString password = passwordEdit_->text();

    if (server.isEmpty() || username.isEmpty() || password.isEmpty()) {
        log("错误：请填写服务器地址、用户名和登录密码。");
        return;
    }

    setFormEnabled(false);
    progressBar_->setVisible(true);
    table_->setRowCount(0);
    log(QString("[%1] 正在获取备份列表...").arg(QDateTime::currentDateTime().toString("hh:mm:ss")));

    auto* thread = new QThread(this);
    auto* worker = new RemoteListWorker(server,
                                        static_cast<uint16_t>(portSpin_->value()),
                                        username, password);
    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker, &RemoteListWorker::run);
    connect(worker, &RemoteListWorker::started, this, &RemoteListTab::onWorkerStarted);
    connect(worker, &RemoteListWorker::finished, this, &RemoteListTab::onWorkerFinished);
    connect(worker, &RemoteListWorker::finished, thread, &QThread::quit);
    connect(worker, &RemoteListWorker::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);

    thread->start();
}

void RemoteListTab::onWorkerStarted() {
    log("正在连接服务器...");
}

void RemoteListTab::onWorkerFinished(bool success, const QString& message,
                                      const QVector<BackupEntry>& entries) {
    progressBar_->setVisible(false);
    setFormEnabled(true);

    if (success) {
        log(QString("✓ %1").arg(message));
        table_->setRowCount(entries.size());
        for (int i = 0; i < entries.size(); ++i) {
            // 名称列：显示用户设置的名称或自动生成提示
            QString displayName = entries[i].name.isEmpty()
                ? "(自动生成)" : entries[i].name;
            auto* nameItem = new QTableWidgetItem(displayName);
            auto* idItem = new QTableWidgetItem(entries[i].id);
            QDateTime dt = QDateTime::fromSecsSinceEpoch(
                static_cast<qint64>(entries[i].timestamp));
            auto* tsItem = new QTableWidgetItem(dt.toString("yyyy-MM-dd hh:mm:ss"));
            table_->setItem(i, 0, nameItem);
            table_->setItem(i, 1, idItem);
            table_->setItem(i, 2, tsItem);
        }
        table_->resizeColumnsToContents();
    } else {
        log(QString("✗ %1").arg(message));
    }
}

void RemoteListTab::setFormEnabled(bool enabled) {
    serverEdit_->setEnabled(enabled);
    portSpin_->setEnabled(enabled);
    usernameEdit_->setEnabled(enabled);
    passwordEdit_->setEnabled(enabled);
    refreshBtn_->setEnabled(enabled);
}

void RemoteListTab::log(const QString& msg) {
    logView_->append(msg);
}
