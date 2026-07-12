#ifndef GUI_LOCALBACKUPTAB_H
#define GUI_LOCALBACKUPTAB_H

#include <QWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QProgressBar>
#include <QTextEdit>

#include "StrategySetup.h"

/// 本地备份选项卡
class LocalBackupTab : public QWidget {
    Q_OBJECT
public:
    explicit LocalBackupTab(QWidget* parent = nullptr);

private slots:
    void onBrowseSource();
    void onBrowseOutputDir();
    void onStartBackup();
    void onWorkerStarted();
    void onWorkerFinished(bool success, const QString& message);

private:
    void setFormEnabled(bool enabled);
    void log(const QString& msg);

    // 表单控件
    QLineEdit* sourceDirEdit_;
    QLineEdit* outputDirEdit_;
    QLineEdit* outputNameEdit_;
    QComboBox* packCombo_;
    QComboBox* compressCombo_;
    QComboBox* encryptCombo_;
    QLineEdit* passwordEdit_;

    // 操作控件
    QPushButton* startBtn_;
    QProgressBar* progressBar_;
    QTextEdit* logView_;

    // 策略（持久化，供 Worker 使用）
    StrategySetup strategies_;
};

#endif // GUI_LOCALBACKUPTAB_H
