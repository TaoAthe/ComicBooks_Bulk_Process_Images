#ifndef APISERVER_H
#define APISERVER_H

#include <QObject>
#include <QTcpServer>
#include <QHash>
#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QUrlQuery>
#include <QString>

class QTcpSocket;

class BackendController;

class ApiServer : public QObject {
    Q_OBJECT

public:
    explicit ApiServer(BackendController *backend, QObject *parent = nullptr);
    bool start(quint16 port = 5500);

private slots:
    void handleNewConnection();

private:
    struct PendingRequest {
        QByteArray buffer;
    };

    void handleReadyRead(QTcpSocket *socket);
    bool isRequestComplete(const QByteArray &data, int &headerEnd, int &contentLength) const;
    void respondJson(QTcpSocket *socket, const QJsonObject &json, int statusCode = 200);
    void respondJson(QTcpSocket *socket, const QJsonArray &json, int statusCode = 200);
    void respondBytes(QTcpSocket *socket, const QByteArray &bytes, const QByteArray &contentType, int statusCode = 200);
    void respondEmpty(QTcpSocket *socket, int statusCode = 200);
    void respondError(QTcpSocket *socket, int statusCode, const QString &message);

    void dispatchRequest(QTcpSocket *socket, const QString &method, const QString &path,
                         const QHash<QString, QString> &headers, const QByteArray &body);

    void handleGet(QTcpSocket *socket, const QString &path, const QUrlQuery &query);
    void handlePost(QTcpSocket *socket, const QString &path, const QByteArray &body);
    void handleDelete(QTcpSocket *socket, const QString &path, const QUrlQuery &query);
    void handleOptions(QTcpSocket *socket);

    void sendCorsHeaders(QByteArray &response) const;

    BackendController *m_backend;
    QTcpServer m_server;
    QHash<QTcpSocket*, PendingRequest> m_pending;
};

#endif // APISERVER_H