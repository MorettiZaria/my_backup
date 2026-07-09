#include "workers/RestoreWorker.h"
#include "core/RestoreEngine.h"
#include <QDebug>
#include <stdexcept>

RestoreWorker::RestoreWorker(const QString& inputFile,
                               const QString& destDir,
                               PackManager* packMgr,
                               CompressManager* compressMgr,
                               EncryptManager* encryptMgr,
                               const QString& password,
                               QObject* parent)
    : QObject(parent)
    , inputFile_(inputFile)
    , destDir_(destDir)
    , packMgr_(packMgr)
    , compressMgr_(compressMgr)
    , encryptMgr_(encryptMgr)
    , password_(password) {}

void RestoreWorker::run() {
    emit started();

    RestoreEngine engine;
    engine.setPackManager(packMgr_);
    engine.setCompressManager(compressMgr_);
    engine.setEncryptManager(encryptMgr_);

    try {
        bool ok = engine.run(inputFile_.toStdString(),
                             destDir_.toStdString(),
                             password_.toStdString());

        if (ok) {
            emit finished(true, QString("还原完成！\n目标目录: %1").arg(destDir_));
        } else {
            emit finished(false, "还原失败，请检查日志输出。");
        }
    } catch (const std::exception& e) {
        emit finished(false, QString("还原过程中发生异常:\n%1\n\n可能原因：密码错误、文件损坏或格式不匹配。")
                           .arg(e.what()));
    } catch (...) {
        emit finished(false, "还原过程中发生未知异常。\n可能原因：密码错误、文件损坏或格式不匹配。");
    }
}
