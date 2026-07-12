#ifndef GUI_RESTOREWORKER_H
#define GUI_RESTOREWORKER_H

#include <QObject>
#include <QString>

class PackManager;
class CompressManager;
class EncryptManager;

/// 本地还原工作线程：在 QThread 中执行 RestoreEngine::run()
class RestoreWorker : public QObject {
    Q_OBJECT
public:
    explicit RestoreWorker(const QString& inputFile,
                           const QString& destDir,
                           PackManager* packMgr,
                           CompressManager* compressMgr,
                           EncryptManager* encryptMgr,
                           const QString& password,
                           QObject* parent = nullptr);

public slots:
    void run();

signals:
    void started();
    void finished(bool success, const QString& message);

private:
    QString inputFile_;
    QString destDir_;
    PackManager* packMgr_;
    CompressManager* compressMgr_;
    EncryptManager* encryptMgr_;
    QString password_;
};

#endif // GUI_RESTOREWORKER_H
