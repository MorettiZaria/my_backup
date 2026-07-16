#ifndef GUI_BACKUPWORKER_H
#define GUI_BACKUPWORKER_H

#include <QObject>
#include <QString>
#include "filter/CompositeFilter.h"

class IPackStrategy;
class ICompressStrategy;
class IEncryptStrategy;

/// 本地备份工作线程：在 QThread 中执行 BackupEngine::run()
class BackupWorker : public QObject {
    Q_OBJECT
public:
    explicit BackupWorker(const QString& sourceDir,
                          const QString& outputFile,
                          IPackStrategy* pack,
                          ICompressStrategy* compress,
                          IEncryptStrategy* encrypt,
                          const QString& password,
                          const CompositeFilter& filter,
                          QObject* parent = nullptr);

public slots:
    void run();

signals:
    void started();
    void finished(bool success, const QString& message);

private:
    QString sourceDir_;
    QString outputFile_;
    IPackStrategy* pack_;
    ICompressStrategy* compress_;
    IEncryptStrategy* encrypt_;
    QString password_;
    CompositeFilter filter_;
};

#endif // GUI_BACKUPWORKER_H
