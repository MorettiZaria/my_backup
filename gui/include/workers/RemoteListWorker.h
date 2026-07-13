#ifndef GUI_REMOTELISTWORKER_H
#define GUI_REMOTELISTWORKER_H

#include <QObject>
#include <QString>
#include <QVector>
#include <cstdint>

/// 备份列表条目
struct BackupEntry {
    QString id;
    QString name;        // 用户设置的备份名称
    uint64_t timestamp;  // Unix 时间戳
};

/// 远程列表工作线程：手动实现网络协议获取备份列表
/// 不修改 NetworkRestoreClient，自己实现 handshake → login → list 流程
class RemoteListWorker : public QObject {
    Q_OBJECT
public:
    explicit RemoteListWorker(const QString& host,
                              uint16_t port,
                              const QString& username,
                              const QString& password,
                              QObject* parent = nullptr);

public slots:
    void run();

signals:
    void started();
    void finished(bool success, const QString& message,
                  const QVector<BackupEntry>& entries);

private:
    QString host_;
    uint16_t port_;
    QString username_;
    QString password_;
};

#endif // GUI_REMOTELISTWORKER_H
