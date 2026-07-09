#ifndef GUI_REMOTERESTORETAB_H
#define GUI_REMOTERESTORETAB_H

#include <QWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QProgressBar>
#include <QTextEdit>

/// 远程还原选项卡
class RemoteRestoreTab : public QWidget {
    Q_OBJECT
public:
    explicit RemoteRestoreTab(QWidget* parent = nullptr);

private slots:
    void onBrowseDest();
    void onStartRestore();
    void onWorkerStarted();
    void onWorkerFinished(bool success, const QString& message);

private:
    void setFormEnabled(bool enabled);
    void log(const QString& msg);

    QLineEdit* serverEdit_;
    QSpinBox* portSpin_;
    QLineEdit* usernameEdit_;
    QLineEdit* passwordEdit_;
    QLineEdit* destDirEdit_;
    QLineEdit* backupIdEdit_;
    QLineEdit* filePasswordEdit_;

    QPushButton* startBtn_;
    QProgressBar* progressBar_;
    QTextEdit* logView_;
};

#endif // GUI_REMOTERESTORETAB_H
