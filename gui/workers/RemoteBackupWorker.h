#ifndef GUI_REMOTEBACKUPWORKER_H
#define GUI_REMOTEBACKUPWORKER_H

#include <QObject>
#include <QString>

class IPackStrategy;
class ICompressStrategy;
class IEncryptStrategy;

/// 远程备份工作线程：在 QThread 中执行 NetworkBackupClient::run()
class RemoteBackupWorker : public QObject {
    Q_OBJECT
public:
    explicit RemoteBackupWorker(const QString& host,
                                uint16_t port,
                                const QString& username,
                                const QString& password,
                                const QString& sourceDir,
                                IPackStrategy* pack,
                                ICompressStrategy* compress,
                                IEncryptStrategy* encrypt,
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
    QString sourceDir_;
    IPackStrategy* pack_;
    ICompressStrategy* compress_;
    IEncryptStrategy* encrypt_;
    QString filePassword_;
};

#endif // GUI_REMOTEBACKUPWORKER_H
