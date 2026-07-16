#include "workers/RemoteBackupWorker.h"
#include "network/NetworkBackupClient.h"
#include <QDebug>

RemoteBackupWorker::RemoteBackupWorker(const QString& host,
                                         uint16_t port,
                                         const QString& username,
                                         const QString& password,
                                         const QString& sourceDir,
                                         IPackStrategy* pack,
                                         ICompressStrategy* compress,
                                         IEncryptStrategy* encrypt,
                                         const QString& filePassword,
                                         const QString& backupName,
                                         const CompositeFilter& filter,
                                         QObject* parent)
    : QObject(parent)
    , host_(host)
    , port_(port)
    , username_(username)
    , password_(password)
    , sourceDir_(sourceDir)
    , pack_(pack)
    , compress_(compress)
    , encrypt_(encrypt)
    , filePassword_(filePassword)
    , backupName_(backupName)
    , filter_(filter) {}

void RemoteBackupWorker::run() {
    emit started();

    NetworkBackupClient client(host_.toStdString(), port_,
                                username_.toStdString(),
                                password_.toStdString());

    if (!backupName_.isEmpty()) {
        client.setBackupName(backupName_.toStdString());
    }
    if (!filter_.isEmpty()) client.setFileFilter(&filter_);

    bool ok = client.run(sourceDir_.toStdString(),
                         pack_, compress_, encrypt_,
                         filePassword_.toStdString());

    if (ok) {
        emit finished(true, "远程备份完成！");
    } else {
        emit finished(false, "远程备份失败，请检查日志输出。");
    }
}
