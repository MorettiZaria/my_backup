#ifndef GUI_REMOTERESTOREWORKER_H
#define GUI_REMOTERESTOREWORKER_H

#include <QObject>
#include <QString>

/// 远程还原工作线程：在 QThread 中执行 NetworkRestoreClient::run()
class RemoteRestoreWorker : public QObject {
    Q_OBJECT
public:
    explicit RemoteRestoreWorker(const QString& host,
                                 uint16_t port,
                                 const QString& username,
                                 const QString& password,
                                 const QString& destDir,
                                 const QString& backupId,
                                 const QString& filePassword,
                                 QObject* parent = nullptr);

public slots:
    void run();

signals:
    void started();
    void finished(bool success, const QString& message);

private:
    QString host_;
    uint16_t port_;
    QString username_;
    QString password_;
    QString destDir_;
    QString backupId_;
    QString filePassword_;
};

#endif // GUI_REMOTERESTOREWORKER_H
