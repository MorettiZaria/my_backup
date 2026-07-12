#ifndef GUI_LOCALRESTORETAB_H
#define GUI_LOCALRESTORETAB_H

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QTextEdit>

#include "StrategySetup.h"

/// 本地还原选项卡
class LocalRestoreTab : public QWidget {
    Q_OBJECT
public:
    explicit LocalRestoreTab(QWidget* parent = nullptr);

private slots:
    void onBrowseInput();
    void onBrowseDest();
    void onStartRestore();
    void onWorkerStarted();
    void onWorkerFinished(bool success, const QString& message);

private:
    void setFormEnabled(bool enabled);
    void log(const QString& msg);

    QLineEdit* inputFileEdit_;
    QLineEdit* destDirEdit_;
    QLineEdit* passwordEdit_;
    QPushButton* startBtn_;
    QProgressBar* progressBar_;
    QTextEdit* logView_;

    StrategySetup strategies_;
};

#endif // GUI_LOCALRESTORETAB_H
