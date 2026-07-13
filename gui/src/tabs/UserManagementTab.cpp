#include "tabs/UserManagementTab.h"
#include "workers/RegisterWorker.h"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QThread>
#include <QDateTime>

UserManagementTab::UserManagementTab(QWidget* parent)
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

    // 新用户信息
    usernameEdit_ = new QLineEdit;
    usernameEdit_->setPlaceholderText("输入新用户名");
    form->addRow("用户名:", usernameEdit_);

    passwordEdit_ = new QLineEdit;
    passwordEdit_->setEchoMode(QLineEdit::Password);
    passwordEdit_->setPlaceholderText("输入密码");
    form->addRow("密码:", passwordEdit_);

    confirmPasswordEdit_ = new QLineEdit;
    confirmPasswordEdit_->setEchoMode(QLineEdit::Password);
    confirmPasswordEdit_->setPlaceholderText("再次输入密码");
    form->addRow("确认密码:", confirmPasswordEdit_);

    mainLayout->addLayout(form);

    // 注册按钮
    registerBtn_ = new QPushButton("▶ 注册用户");
    registerBtn_->setStyleSheet("QPushButton { font-size: 14px; padding: 8px 16px; }");
    mainLayout->addWidget(registerBtn_);
    connect(registerBtn_, &QPushButton::clicked, this, &UserManagementTab::onRegister);

    // 进度条
    progressBar_ = new QProgressBar;
    progressBar_->setRange(0, 0);
    progressBar_->setVisible(false);
    mainLayout->addWidget(progressBar_);

    // 日志
    logView_ = new QTextEdit;
    logView_->setReadOnly(true);
    logView_->document()->setMaximumBlockCount(1000);
    logView_->setPlaceholderText("操作日志将显示在这里...");
    mainLayout->addWidget(logView_);

    mainLayout->addStretch();
}

void UserManagementTab::onRegister() {
    QString server = serverEdit_->text().trimmed();
    QString username = usernameEdit_->text().trimmed();
    QString password = passwordEdit_->text();
    QString confirm = confirmPasswordEdit_->text();

    // 输入校验
    if (server.isEmpty()) {
        log("错误：请填写服务器地址。");
        return;
    }
    if (username.isEmpty()) {
        log("错误：请填写用户名。");
        return;
    }
    if (password.isEmpty()) {
        log("错误：请填写密码。");
        return;
    }
    if (password != confirm) {
        log("错误：两次输入的密码不一致。");
        return;
    }
    if (password.length() < 4) {
        log("错误：密码长度不能少于 4 个字符。");
        return;
    }

    setFormEnabled(false);
    progressBar_->setVisible(true);
    logView_->clear();
    log(QString("[%1] 开始注册用户...").arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
    log(QString("  服务器: %1:%2").arg(server).arg(portSpin_->value()));
    log(QString("  用户名: %1").arg(username));

    auto* thread = new QThread(this);
    auto* worker = new RegisterWorker(server,
                                       static_cast<uint16_t>(portSpin_->value()),
                                       username, password);
    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker, &RegisterWorker::run);
    connect(worker, &RegisterWorker::started, this, &UserManagementTab::onWorkerStarted);
    connect(worker, &RegisterWorker::finished, this, &UserManagementTab::onWorkerFinished);
    connect(worker, &RegisterWorker::finished, thread, &QThread::quit);
    connect(worker, &RegisterWorker::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);

    thread->start();
}

void UserManagementTab::onWorkerStarted() {
    log("正在连接服务器并注册用户...");
}

void UserManagementTab::onWorkerFinished(bool success, const QString& message) {
    progressBar_->setVisible(false);
    setFormEnabled(true);

    if (success) {
        log(QString("✓ %1").arg(message));
    } else {
        log(QString("✗ %1").arg(message));
    }
}

void UserManagementTab::setFormEnabled(bool enabled) {
    serverEdit_->setEnabled(enabled);
    portSpin_->setEnabled(enabled);
    usernameEdit_->setEnabled(enabled);
    passwordEdit_->setEnabled(enabled);
    confirmPasswordEdit_->setEnabled(enabled);
    registerBtn_->setEnabled(enabled);
}

void UserManagementTab::log(const QString& msg) {
    logView_->append(msg);
}
