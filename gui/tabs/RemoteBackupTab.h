#ifndef GUI_REMOTEBACKUPTAB_H
#define GUI_REMOTEBACKUPTAB_H

#include <QWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QProgressBar>
#include <QTextEdit>

#include "StrategySetup.h"

/// 远程备份选项卡
class RemoteBackupTab : public QWidget {
    Q_OBJECT
public:
    explicit RemoteBackupTab(QWidget* parent = nullptr);

private slots:
    void onBrowseSource();
    void onStartBackup();
    void onWorkerStarted();
    void onWorkerFinished(bool success, const QString& message);

private:
    void setFormEnabled(bool enabled);
    void log(const QString& msg);

    QLineEdit* serverEdit_;
    QSpinBox* portSpin_;
    QLineEdit* usernameEdit_;
    QLineEdit* passwordEdit_;       // 登录密码
    QLineEdit* sourceDirEdit_;
    QComboBox* packCombo_;
    QComboBox* compressCombo_;
    QComboBox* encryptCombo_;
    QLineEdit* filePasswordEdit_;   // 文件加密密码

    QPushButton* startBtn_;
    QProgressBar* progressBar_;
    QTextEdit* logView_;

    StrategySetup strategies_;
};

#endif // GUI_REMOTEBACKUPTAB_H
