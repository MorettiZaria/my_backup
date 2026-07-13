#ifndef GUI_REGISTERWORKER_H
#define GUI_REGISTERWORKER_H

#include <QObject>
#include <QString>

/// 用户注册工作线程：连接服务器并注册新用户
class RegisterWorker : public QObject {
    Q_OBJECT
public:
    explicit RegisterWorker(const QString& host,
                            uint16_t port,
                            const QString& username,
                            const QString& password,
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
};

#endif // GUI_REGISTERWORKER_H
