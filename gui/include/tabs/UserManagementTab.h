#ifndef GUI_USERMANAGEMENTTAB_H
#define GUI_USERMANAGEMENTTAB_H

#include <QWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QProgressBar>
#include <QTextEdit>

/// 用户管理选项卡：在远程服务器上注册新用户
class UserManagementTab : public QWidget {
    Q_OBJECT
public:
    explicit UserManagementTab(QWidget* parent = nullptr);

private slots:
    void onRegister();
    void onWorkerStarted();
    void onWorkerFinished(bool success, const QString& message);

private:
    void setFormEnabled(bool enabled);
    void log(const QString& msg);

    QLineEdit* serverEdit_;
    QSpinBox* portSpin_;
    QLineEdit* usernameEdit_;
    QLineEdit* passwordEdit_;
    QLineEdit* confirmPasswordEdit_;

    QPushButton* registerBtn_;
    QProgressBar* progressBar_;
    QTextEdit* logView_;
};

#endif // GUI_USERMANAGEMENTTAB_H
