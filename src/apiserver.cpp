#include "apiserver.h"

#include "backendcontroller.h"

#include <QTcpSocket>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <QStringList>
#include <QPointer>
#include <QDebug>

namespace {
constexpr auto kJson = "application/json";
constexpr auto kCorsAllowOrigin = "Access-Control-Allow-Origin: *\r\n";
constexpr auto kCorsAllowMethods = "Access-Control-Allow-Methods: GET, POST, DELETE, OPTIONS\r\n";
constexpr auto kCorsAllowHeaders = "Access-Control-Allow-Headers: Content-Type\r\n";
}

ApiServer::ApiServer(BackendController *backend, QObject *parent)
    : QObject(parent),
      m_backend(backend) {
    connect(&m_server, &QTcpServer::newConnection, this, &ApiServer::handleNewConnection);
}

bool ApiServer::start(quint16 port) {
    if (!m_server.listen(QHostAddress::Any, port)) {
        qWarning() << "Failed to start API server on port" << port;
        return false;
    }
    qInfo() << "API server listening on port" << port;
    return true;
}

void ApiServer::handleNewConnection() {
    while (QTcpSocket *socket = m_server.nextPendingConnection()) {
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() { handleReadyRead(socket); });
        connect(socket, &QTcpSocket::disconnected, this, [this, socket]() {
            m_pending.remove(socket);
            socket->deleteLater();
        });
        m_pending.insert(socket, PendingRequest{});
    }
}

bool ApiServer::isRequestComplete(const QByteArray &data, int &headerEnd, int &contentLength) const {
    headerEnd = data.indexOf("\r\n\r\n");
    if (headerEnd == -1) {
        return false;
    }
    contentLength = 0;
    QList<QByteArray> lines = data.left(headerEnd).split('\n');
    for (const QByteArray &line : lines) {
        QByteArray trimmed = line.trimmed();
        if (trimmed.left(15).toLower() == "content-length:") {
            QList<QByteArray> parts = trimmed.split(' ');
            if (parts.size() >= 2) {
                contentLength = parts.last().toInt();
            }
            break;
        }
    }
    int totalLength = headerEnd + 4 + contentLength;
    return data.size() >= totalLength;
}

void ApiServer::handleReadyRead(QTcpSocket *socket) {
    PendingRequest &pending = m_pending[socket];
    pending.buffer.append(socket->readAll());

    int headerEnd = -1;
    int contentLength = 0;
    if (!isRequestComplete(pending.buffer, headerEnd, contentLength)) {
        return;
    }

    QByteArray request = pending.buffer.left(headerEnd + 4 + contentLength);
    pending.buffer.remove(0, headerEnd + 4 + contentLength);

    QList<QByteArray> lines = request.left(headerEnd).split('\n');
    if (lines.isEmpty()) {
        respondError(socket, 400, "Malformed request");
        return;
    }

    QByteArray requestLine = lines.takeFirst().trimmed();
    QList<QByteArray> parts = requestLine.split(' ');
    if (parts.size() < 3) {
        respondError(socket, 400, "Invalid request line");
        return;
    }

    QString method = QString::fromUtf8(parts.at(0));
    QString path = QString::fromUtf8(parts.at(1));

    qInfo() << "API Request:" << method << path;

    QHash<QString, QString> headers;
    for (const QByteArray &line : lines) {
        int colon = line.indexOf(':');
        if (colon > 0) {
            QString key = QString::fromUtf8(line.left(colon)).trimmed().toLower();
            QString value = QString::fromUtf8(line.mid(colon + 1)).trimmed();
            headers.insert(key, value);
        }
    }

    QByteArray body = request.mid(headerEnd + 4, contentLength);
    dispatchRequest(socket, method, path, headers, body);
}

void ApiServer::dispatchRequest(QTcpSocket *socket, const QString &method, const QString &path,
                                const QHash<QString, QString> &, const QByteArray &body) {
    if (method.compare("OPTIONS", Qt::CaseInsensitive) == 0) {
        handleOptions(socket);
        return;
    }

    QUrl url(path);
    QString cleanPath = url.path();
    QUrlQuery query(url);

    if (method.compare("GET", Qt::CaseInsensitive) == 0) {
        handleGet(socket, cleanPath, query);
    } else if (method.compare("POST", Qt::CaseInsensitive) == 0) {
        handlePost(socket, cleanPath, body);
    } else if (method.compare("DELETE", Qt::CaseInsensitive) == 0) {
        handleDelete(socket, cleanPath, query);
    } else {
        respondError(socket, 405, "Method Not Allowed");
    }
}

void ApiServer::handleGet(QTcpSocket *socket, const QString &path, const QUrlQuery &query) {
    if (path == "/api/status") {
        respondJson(socket, m_backend->status());
        return;
    }
    if (path == "/api/context") {
        QJsonObject json;
        json["rootPath"] = m_backend->rootPath();
        json["currentPath"] = m_backend->currentPath();
        respondJson(socket, json);
        return;
    }
    if (path == "/api/folders") {
        QString requested = query.queryItemValue("path", QUrl::FullyDecoded);
        if (!requested.isEmpty()) {
            m_backend->setCurrentPath(requested);
        }
        respondJson(socket, m_backend->directoryListing(requested));
        return;
    }
    if (path == "/api/queue") {
        respondJson(socket, m_backend->queueState());
        return;
    }
    if (path == "/api/comics") {
        respondJson(socket, m_backend->processedComics());
        return;
    }
    if (path == "/api/export/csv") {
        QByteArray csv = m_backend->exportCsv();
        QByteArray response;
        response.append("HTTP/1.1 200 OK\r\n");
        sendCorsHeaders(response);
        response.append("Content-Type: text/csv; charset=utf-8\r\n");
        response.append("Content-Disposition: attachment; filename=\"comics-ebay.csv\"\r\n");
        response.append("Content-Length: " + QByteArray::number(csv.size()) + "\r\n");
        response.append("Connection: close\r\n\r\n");
        response.append(csv);
        socket->write(response);
        socket->disconnectFromHost();
        return;
    }
    if (path == "/gallery") {
        respondJson(socket, m_backend->gallery());
        return;
    }
    if (path == "/image") {
        bool ok = false;
        int id = query.queryItemValue("id").toInt(&ok);
        if (!ok) {
            respondError(socket, 400, "Missing id parameter");
            return;
        }
        QString mimeType;
        QByteArray image = m_backend->imageById(id, mimeType);
        if (image.isEmpty()) {
            respondError(socket, 404, "Image not found");
            return;
        }
        respondBytes(socket, image, mimeType.toUtf8());
        return;
    }
    if (path == "/api/models") {
        QString server = query.queryItemValue("server");
        ComicProcessor::ServerType type =
            server.compare("Ollama",  Qt::CaseInsensitive) == 0 ? ComicProcessor::Ollama
          : server.compare("Gemini",  Qt::CaseInsensitive) == 0 ? ComicProcessor::Gemini
          : ComicProcessor::LMStudio;
        QPointer<QTcpSocket> weakSocket(socket);
        m_backend->requestModels(type, [this, weakSocket](const QStringList &models) {
            if (!weakSocket) {
                return;
            }
            QJsonArray array;
            for (const QString &model : models) {
                array.append(model);
            }
            respondJson(weakSocket.data(), array);
        });
        return;
    }

    respondError(socket, 404, "Not Found");
}

void ApiServer::handlePost(QTcpSocket *socket, const QString &path, const QByteArray &body) {
    QJsonParseError parseError{};
    QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);

    if ((path == "/api/queue" || path == "/api/process" || path == "/api/comics/update") && parseError.error != QJsonParseError::NoError) {
        respondError(socket, 400, QStringLiteral("Invalid JSON: %1").arg(parseError.errorString()));
        return;
    }

    if (path == "/api/queue") {
        QStringList paths;
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj.contains("paths")) {
                for (const QJsonValue &value : obj.value("paths").toArray()) {
                    paths.append(value.toString());
                }
            } else if (obj.contains("path")) {
                paths.append(obj.value("path").toString());
            }
        }
        QStringList errors;
        bool added = m_backend->addToQueue(paths, errors);
        QJsonObject response;
        response["added"] = added;
        QJsonArray errorArray;
        for (const QString &err : errors) {
            errorArray.append(err);
        }
        response["errors"] = errorArray;
        respondJson(socket, response);
        return;
    }

    if (path == "/api/process") {
        if (!doc.isObject()) {
            respondError(socket, 400, "Process payload must be JSON object");
            return;
        }
        QJsonObject obj = doc.object();
        QString server = obj.value("server").toString("LMStudio");
        QString model = obj.value("model").toString();
        bool generateEbay = obj.value("generateEbay").toBool(false);
        ComicProcessor::ServerType type =
            server.compare("Ollama",  Qt::CaseInsensitive) == 0 ? ComicProcessor::Ollama
          : server.compare("Gemini",  Qt::CaseInsensitive) == 0 ? ComicProcessor::Gemini
          : ComicProcessor::LMStudio;
        m_backend->configureProcessing(type, model, generateEbay);
        m_backend->startProcessing();
        respondJson(socket, m_backend->status());
        return;
    }

    if (path == "/api/comics/update") {
        if (!doc.isObject()) {
            respondError(socket, 400, "Update payload must be JSON object");
            return;
        }
        QJsonObject obj = doc.object();
        int id = obj.value("id").toInt(-1);
        if (id <= 0) {
            respondError(socket, 400, "Missing or invalid id");
            return;
        }
        if (!obj.contains("updates") || !obj.value("updates").isObject()) {
            respondError(socket, 400, "Missing updates object");
            return;
        }
        QStringList errors;
        bool updated = m_backend->updateComic(id, obj.value("updates").toObject(), errors);
        QJsonObject response;
        response["updated"] = updated;
        if (!errors.isEmpty()) {
            QJsonArray errs;
            for (const QString &err : errors) {
                errs.append(err);
            }
            response["errors"] = errs;
        }
        if (updated) {
            respondJson(socket, response);
        } else {
            respondJson(socket, response, 400);
        }
        return;
    }

    respondError(socket, 404, "Not Found");
}

void ApiServer::handleDelete(QTcpSocket *socket, const QString &path, const QUrlQuery &query) {
    if (path == "/api/queue") {
        QString pathParam = query.queryItemValue("path", QUrl::FullyDecoded);
        if (pathParam.isEmpty()) {
            respondError(socket, 400, "Missing path parameter");
            return;
        }
        bool removed = m_backend->removeFromQueue(pathParam);
        QJsonObject json;
        json["removed"] = removed;
        respondJson(socket, json);
        return;
    }
    if (path == "/api/queue/all") {
        m_backend->clearQueue();
        respondEmpty(socket);
        return;
    }
    if (path == "/api/comics") {
        bool ok = false;
        int id = query.queryItemValue("id").toInt(&ok);
        if (!ok || id <= 0) {
            respondError(socket, 400, "Missing or invalid id parameter");
            return;
        }
        bool removed = m_backend->deleteComic(id);
        if (!removed) {
            respondError(socket, 404, "Comic not found");
            return;
        }
        respondEmpty(socket);
        return;
    }
    respondError(socket, 404, "Not Found");
}

void ApiServer::handleOptions(QTcpSocket *socket) {
    QByteArray response;
    response.append("HTTP/1.1 204 No Content\r\n");
    sendCorsHeaders(response);
    response.append("Content-Length: 0\r\n");
    response.append("Connection: close\r\n\r\n");
    socket->write(response);
    socket->disconnectFromHost();
}

void ApiServer::respondJson(QTcpSocket *socket, const QJsonObject &json, int statusCode) {
    respondBytes(socket, QJsonDocument(json).toJson(QJsonDocument::Compact), kJson, statusCode);
}

void ApiServer::respondJson(QTcpSocket *socket, const QJsonArray &json, int statusCode) {
    respondBytes(socket, QJsonDocument(json).toJson(QJsonDocument::Compact), kJson, statusCode);
}

void ApiServer::respondBytes(QTcpSocket *socket, const QByteArray &bytes, const QByteArray &contentType, int statusCode) {
    QByteArray response;
    response.append(QString("HTTP/1.1 %1\r\n").arg(statusCode).toUtf8());
    sendCorsHeaders(response);
    response.append("Content-Type: " + contentType + "\r\n");
    response.append("Content-Length: " + QByteArray::number(bytes.size()) + "\r\n");
    response.append("Connection: close\r\n\r\n");
    response.append(bytes);
    socket->write(response);
    socket->disconnectFromHost();
}

void ApiServer::respondEmpty(QTcpSocket *socket, int statusCode) {
    QByteArray response;
    response.append(QString("HTTP/1.1 %1\r\n").arg(statusCode).toUtf8());
    sendCorsHeaders(response);
    response.append("Content-Length: 0\r\n");
    response.append("Connection: close\r\n\r\n");
    socket->write(response);
    socket->disconnectFromHost();
}

void ApiServer::respondError(QTcpSocket *socket, int statusCode, const QString &message) {
    QJsonObject json;
    json["error"] = message;
    respondJson(socket, json, statusCode);
}

void ApiServer::sendCorsHeaders(QByteArray &response) const {
    response.append(kCorsAllowOrigin);
    response.append(kCorsAllowMethods);
    response.append(kCorsAllowHeaders);
}