#include "workers/BackupWorker.h"
#include "core/BackupEngine.h"
#include <QDebug>

BackupWorker::BackupWorker(const QString& sourceDir,
                             const QString& outputFile,
                             IPackStrategy* pack,
                             ICompressStrategy* compress,
                             IEncryptStrategy* encrypt,
                             const QString& password,
                             QObject* parent)
    : QObject(parent)
    , sourceDir_(sourceDir)
    , outputFile_(outputFile)
    , pack_(pack)
    , compress_(compress)
    , encrypt_(encrypt)
    , password_(password) {}

void BackupWorker::run() {
    emit started();

    BackupEngine engine;
    engine.setPackStrategy(pack_);
    engine.setCompressStrategy(compress_);
    engine.setEncryptStrategy(encrypt_);

    bool ok = engine.run(sourceDir_.toStdString(),
                         outputFile_.toStdString(),
                         password_.toStdString());

    if (ok) {
        emit finished(true, QString("备份完成！\n输出文件: %1").arg(outputFile_));
    } else {
        emit finished(false, "备份失败，请检查日志输出。");
    }
}
