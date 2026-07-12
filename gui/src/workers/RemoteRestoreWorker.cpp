#include "workers/RemoteRestoreWorker.h"
#include "network/NetworkRestoreClient.h"
#include <QDebug>

RemoteRestoreWorker::RemoteRestoreWorker(const QString& host,
                                           uint16_t port,
                                           const QString& username,
                                           const QString& password,
                                           const QString& destDir,
                                           const QString& backupId,
                                           const QString& filePassword,
                                           QObject* parent)
    : QObject(parent)
    , host_(host)
    , port_(port)
    , username_(username)
    , password_(password)
    , destDir_(destDir)
    , backupId_(backupId)
    , filePassword_(filePassword) {}

void RemoteRestoreWorker::run() {
    emit started();

    NetworkRestoreClient client(host_.toStdString(), port_,
                                 username_.toStdString(),
                                 password_.toStdString());

    bool ok = client.run(destDir_.toStdString(),
                         backupId_.toStdString(),
                         filePassword_.toStdString());

    if (ok) {
        emit finished(true, QString("远程还原完成！\n目标目录: %1").arg(destDir_));
    } else {
        emit finished(false, "远程还原失败，请检查日志输出。");
    }
}
