#include "comicprocessor.h"
#include <QDir>
#include <QFile>
#include <QImage>
#include <QByteArray>
#include <QBuffer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QDebug>
#include <QSqlError>

ComicProcessor::ComicProcessor(QObject *parent) : QObject(parent) {
    networkManager = new QNetworkAccessManager(this);
    server = Ollama; // default
    model = "llava"; // default

    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(QCoreApplication::applicationDirPath() + "/comicvision.db");
    if (!db.open()) {
        qDebug() << "Failed to open database";
    } else {
        QSqlQuery query;
        query.exec("CREATE TABLE IF NOT EXISTS processed_images (id INTEGER PRIMARY KEY AUTOINCREMENT, image_path TEXT, metadata TEXT, timestamp DATETIME DEFAULT CURRENT_TIMESTAMP)");
    }
}

QString ComicProcessor::getServerUrl() const {
    if (server == Ollama) {
        return "http://localhost:11434";
    } else {
        return "http://localhost:1234";
    }
}

QStringList ComicProcessor::scanImagesDirectory() {
    QDir dir(QCoreApplication::applicationDirPath() + "/images");
    QStringList filters = {"*.jpg", "*.jpeg", "*.png", "*.webp"};
    return dir.entryList(filters, QDir::Files);
}

void ComicProcessor::processImage(const QString &imagePath, std::function<void(const QString&)> callback) {
    QString fullPath = QCoreApplication::applicationDirPath() + "/images/" + imagePath;
    QString base64 = base64EncodeImage(fullPath);
    if (base64.startsWith("Failed to load")) {
        callback(base64);
        return;
    }
    sendToAI(base64, [this, fullPath, callback](const QString& response) {
        callback(response);
        moveToProcessed(fullPath);
        saveToDB(fullPath, response);
    });
}

QString ComicProcessor::base64EncodeImage(const QString &imagePath) {
    qDebug() << "Loading image:" << imagePath;
    QImageReader reader(imagePath);
    QImage image = reader.read();
    if (image.isNull()) {
        QString errorMsg = "Failed to load image: " + imagePath + ". QImageReader error: " + reader.errorString();
        qDebug() << errorMsg;
        qDebug() << "File exists:" << QFile::exists(imagePath);
        qDebug() << "File size:" << QFileInfo(imagePath).size();
        QFile file(imagePath);
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray data = file.read(10);
            qDebug() << "First 10 bytes:" << data.toHex();
            file.close();
        } else {
            qDebug() << "Cannot open file for reading.";
        }
        return errorMsg;  // Return the error message instead of empty string
    }

    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG"); // Convert to PNG for consistency
    return byteArray.toBase64();
}

void ComicProcessor::sendToAI(const QString &base64Image, std::function<void(const QString&)> callback) {
    QUrl url(getServerUrl() + (server == Ollama ? "/api/generate" : "/v1/chat/completions"));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject json;
    json["model"] = model;
    QString prompt = "Analyze this comic book cover. Extract: title, issue number, variant, publisher, and any notable text.";
    if (!customPrompt.isEmpty()) {
        prompt += " " + customPrompt;
    }

    if (server == Ollama) {
        json["prompt"] = prompt;
        QJsonArray imagesArray;
        imagesArray.append(base64Image);
        json["images"] = imagesArray;
        json["stream"] = false;
    } else { // LMStudio
        QJsonArray messages;
        QJsonObject message;
        message["role"] = "user";
        QJsonArray content;
        QJsonObject textPart;
        textPart["type"] = "text";
        textPart["text"] = prompt;
        content.append(textPart);
        QJsonObject imagePart;
        imagePart["type"] = "image_url";
        QJsonObject imageUrl;
        imageUrl["url"] = "data:image/png;base64," + base64Image;
        imagePart["image_url"] = imageUrl;
        content.append(imagePart);
        message["content"] = content;
        messages.append(message);
        json["messages"] = messages;
    }

    QByteArray data = QJsonDocument(json).toJson();
    qDebug() << "Sending request to:" << url.toString();
    qDebug() << "Request JSON:" << data;

    QNetworkReply *reply = networkManager->post(request, data);
    connect(reply, &QNetworkReply::finished, [reply, callback, this]() {
        if (reply->error() != QNetworkReply::NoError) {
            QString errorMsg = "Network error: " + reply->errorString();
            qDebug() << errorMsg;
            callback(errorMsg);
        } else {
            QByteArray responseData = reply->readAll();
            qDebug() << "Response data:" << responseData;
            QJsonDocument doc = QJsonDocument::fromJson(responseData);
            if (!doc.isNull()) {
                QString response;
                if (server == Ollama) {
                    response = doc.object()["response"].toString();
                } else {
                    QJsonArray choices = doc.object()["choices"].toArray();
                    if (!choices.isEmpty()) {
                        response = choices[0].toObject()["message"].toObject()["content"].toString();
                    }
                }
                callback(response);
            } else {
                QString errorMsg = "Error: Invalid JSON response. Raw response: " + QString(responseData);
                qDebug() << errorMsg;
                callback(errorMsg);
            }
        }
        reply->deleteLater();
    });
}

void ComicProcessor::moveToProcessed(const QString &imagePath) {
    QFile file(imagePath);
    QString newPath = QCoreApplication::applicationDirPath() + "/processed/" + QFileInfo(imagePath).fileName();
    if (!file.rename(newPath)) {
        qDebug() << "Error moving file:" << imagePath;
    }
}

void ComicProcessor::setServer(ServerType s) {
    server = s;
}

void ComicProcessor::setModel(const QString &m) {
    model = m;
}

void ComicProcessor::setCustomPrompt(const QString &p) {
    customPrompt = p;
}

void ComicProcessor::fetchModels(std::function<void(const QStringList&)> callback) {
    QString endpoint = (server == Ollama) ? "/api/tags" : "/v1/models";
    QUrl url(getServerUrl() + endpoint);
    QNetworkRequest request(url);

    QNetworkReply *reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, [reply, callback, this]() {
        QStringList allModels;
        if (reply->error() != QNetworkReply::NoError) {
            qDebug() << "Error fetching models:" << reply->errorString();
        } else {
            QByteArray responseData = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(responseData);
            if (!doc.isNull()) {
                if (server == Ollama) {
                    QJsonArray modelArray = doc.object()["models"].toArray();
                    for (const QJsonValue &value : modelArray) {
                        allModels << value.toObject()["name"].toString();
                    }
                } else {
                    QJsonArray modelArray = doc.object()["data"].toArray();
                    for (const QJsonValue &value : modelArray) {
                        allModels << value.toObject()["id"].toString();
                    }
                }
            }
        }
        reply->deleteLater();

        // Now filter for vision-capable models
        filterVisionModels(allModels, callback);
    });
}

void ComicProcessor::filterVisionModels(const QStringList &allModels, std::function<void(const QStringList&)> callback) {
    if (server == Ollama) {
        // Temporarily skip filtering, return all models
        callback(allModels);
    } else {
        // For LM Studio, filter by name
        QStringList visionModels;
        for (const QString &model : allModels) {
            if (model.contains("vision", Qt::CaseInsensitive) || model.contains("gpt-4", Qt::CaseInsensitive) || model.contains("vl", Qt::CaseInsensitive)) {
                visionModels << model;
            }
        }
        callback(visionModels);
    }
}

void ComicProcessor::fetchModelInfo(const QString &model, std::function<void(const QString&)> callback) {
    if (server == Ollama) {
        QUrl url(getServerUrl() + "/api/show");
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        QJsonObject json;
        json["name"] = model;
        QByteArray data = QJsonDocument(json).toJson();

        QNetworkReply *reply = networkManager->post(request, data);
        connect(reply, &QNetworkReply::finished, [reply, callback, this]() {
            QString info = "Unable to fetch info";
            if (reply->error() == QNetworkReply::NoError) {
                QByteArray responseData = reply->readAll();
                QJsonDocument doc = QJsonDocument::fromJson(responseData);
                if (!doc.isNull()) {
                    if (doc.object().contains("projector_info")) {
                        info = "Supports vision";
                    } else {
                        info = "Text-only model";
                    }
                }
            }
            callback(info);
            reply->deleteLater();
        });
    } else {
        // For LM Studio, assume vision if model name suggests it
        QString info;
        if (model.contains("vision", Qt::CaseInsensitive) || model.contains("gpt-4", Qt::CaseInsensitive) || model.contains("vl", Qt::CaseInsensitive)) {
            info = "Likely supports vision (check model docs)";
        } else {
            info = "Vision support unknown (check LM Studio)";
        }
        callback(info);
    }
}

void ComicProcessor::saveToDB(const QString &imagePath, const QString &metadata) {
    QSqlQuery query;
    query.prepare("INSERT INTO processed_images (image_path, metadata) VALUES (?, ?)");
    query.addBindValue(imagePath);
    query.addBindValue(metadata);
    if (!query.exec()) {
        qDebug() << "DB insert failed:" << query.lastError();
    }
}