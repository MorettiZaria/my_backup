#ifndef GUI_REMOTELISTTAB_H
#define GUI_REMOTELISTTAB_H

#include <QWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QTableWidget>
#include <QProgressBar>
#include <QTextEdit>
#include <QVector>

#include "workers/RemoteListWorker.h"

/// 远程备份列表选项卡
class RemoteListTab : public QWidget {
    Q_OBJECT
public:
    explicit RemoteListTab(QWidget* parent = nullptr);

private slots:
    void onRefresh();
    void onWorkerStarted();
    void onWorkerFinished(bool success, const QString& message,
                          const QVector<BackupEntry>& entries);

private:
    void setFormEnabled(bool enabled);
    void log(const QString& msg);

    QLineEdit* serverEdit_;
    QSpinBox* portSpin_;
    QLineEdit* usernameEdit_;
    QLineEdit* passwordEdit_;

    QPushButton* refreshBtn_;
    QTableWidget* table_;
    QProgressBar* progressBar_;
    QTextEdit* logView_;
};

#endif // GUI_REMOTELISTTAB_H
