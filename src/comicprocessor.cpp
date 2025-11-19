#include "comicprocessor.h"
#include <QDir>
#include <QFile>
#include <QImage>
#include <QImageReader>
#include <QByteArray>
#include <QBuffer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QDebug>
#include <QSqlError>
#include <QMessageBox>
#include <QUuid>
#include <QProcess>
#include <QTemporaryFile>

ComicProcessor::ComicProcessor(QObject *parent) : QObject(parent) {
    networkManager = new QNetworkAccessManager(this);
    server = LMStudio; // default
    model = ""; // default

    // Ensure directories exist
    QDir().mkpath(QCoreApplication::applicationDirPath() + "/images");
    QDir().mkpath(QCoreApplication::applicationDirPath() + "/processed");

    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(QCoreApplication::applicationDirPath() + "/comicvision.db");
    if (!db.open()) {
        qDebug() << "Failed to open database";
    } else {
        QSqlQuery query(db);
        query.exec("CREATE TABLE IF NOT EXISTS comics (id INTEGER PRIMARY KEY AUTOINCREMENT, image_path TEXT, image_data BLOB, action TEXT, category TEXT, title TEXT, issue TEXT, publisher TEXT, year TEXT, genre TEXT, main_characters TEXT, condition TEXT, value TEXT, notes TEXT)");
        qDebug() << "Create table error:" << query.lastError().text();
        // Check if image_data column exists, if not, add it
        query.exec("PRAGMA table_info(comics)");
        bool hasImageData = false;
        while (query.next()) {
            if (query.value(1).toString() == "image_data") {
                hasImageData = true;
                break;
            }
        }
        if (!hasImageData) {
            query.exec("ALTER TABLE comics ADD COLUMN image_data BLOB");
            qDebug() << "Added image_data column";
        }
    }
}

ComicProcessor::~ComicProcessor() {
    // Abort all pending network requests
    QList<QNetworkReply*> replies = findChildren<QNetworkReply*>();
    for (QNetworkReply *reply : replies) {
        reply->abort();
    }
    if (db.isOpen()) db.close();
}

void ComicProcessor::abortRequests() {
    if (networkManager) {
        networkManager->deleteLater();
        networkManager = nullptr;
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
    QByteArrayList supportedFormats = QImageReader::supportedImageFormats();
    QStringList filters;
    for (const QByteArray &format : supportedFormats) {
        filters << "*." + QString(format);
    }
    // Add webp even if not supported, since we handle it specially
    if (!supportedFormats.contains("webp")) {
        filters << "*.webp";
    }
    QStringList files = dir.entryList(filters, QDir::Files);
    QStringList validFiles;
    for (const QString &file : files) {
        QFileInfo fi(dir.absoluteFilePath(file));
        if (fi.exists() && fi.isFile()) {
            validFiles << fi.absoluteFilePath();
        }
    }
    return validFiles;
}

void ComicProcessor::processImage(const QString &imagePath, std::function<void(const QString&)> callback) {
    QString fullPath = imagePath; // imagePath is already full path from scanImagesDirectory
    QString base64 = base64EncodeImage(fullPath);
    if (base64.startsWith("Failed to load")) {
        callback(base64);
        return;
    }
    sendToAI(base64, [this, fullPath, callback](const QString& response) {
        callback(response);
        ComicEntry entry = parseResponse(response);
        entry.imagePath = fullPath;
        entry.picture_url = fullPath;
        saveToDB(entry);
        if (!response.startsWith("Error")) {
            moveToProcessed(fullPath);
        }
    });
}

QString ComicProcessor::base64EncodeImage(const QString &imagePath) {
    qDebug() << "Loading image:" << imagePath;
    // Check if it's WebP
    QFile file(imagePath);
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray header = file.read(12);
        file.close();
        if (header.startsWith("RIFF") && header.mid(8, 4) == "WEBP") {
            // For WebP, return raw base64 with prefix
            if (file.open(QIODevice::ReadOnly)) {
                QByteArray data = file.readAll();
                file.close();
                return "WEBP:" + data.toBase64();
            }
        }
    }
    // For other formats, load and convert to PNG
    QImage image;
    bool loaded = image.load(imagePath);
    if (!loaded) {
        // Try loading as WebP if it looks like WebP (already checked above)
        QString errorMsg = "Failed to load image: " + imagePath + ". QImage error: Image is null";
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
        return errorMsg;
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

    QString templateStr = R"(<html><body><h2>Comic Book Details</h2><p><strong>Title:</strong> [title]</p><p><strong>Issue:</strong> [issue]</p><p><strong>Publisher:</strong> [publisher]</p><p><strong>Year:</strong> [year]</p><p><strong>Genre:</strong> [genre]</p><p><strong>Main Characters:</strong> [characters]</p><p><strong>Condition:</strong> [condition]</p><p><strong>Value:</strong> [value]</p><p><strong>Notes:</strong> [notes]</p></body></html>)";

    QJsonObject json;
    json["model"] = model;
    QString prompt = "Analyze this comic book cover. Create an eBay listing title (max 80 characters) with key book info. Extract: issue number, variant, publisher, condition, value. Generate detailed notes as an HTML template with the extracted info filled in. Use this template for notes:\n\n" + templateStr + "\n\nOutput in format: Title: [title]\nIssue: [issue]\nVariant: [variant]\nPublisher: [publisher]\nCondition: [condition]\nValue: [value]\nNotes: [html notes]";
    if (!customPrompt.isEmpty()) {
        prompt += " " + customPrompt;
    }

    if (server == Ollama) {
        json["prompt"] = prompt;
        QJsonArray imagesArray;
        QString actualBase64 = base64Image.startsWith("WEBP:") ? base64Image.mid(5) : base64Image;
        imagesArray.append(actualBase64);
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
        QString mime = base64Image.startsWith("WEBP:") ? "data:image/webp;base64," : "data:image/png;base64,";
        QString actualBase64 = base64Image.startsWith("WEBP:") ? base64Image.mid(5) : base64Image;
        imageUrl["url"] = mime + actualBase64;
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
    if (!QFile::exists(imagePath)) {
        return; // Already moved or doesn't exist
    }
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
            if (model.contains("vision", Qt::CaseInsensitive) || model.contains("gpt-4", Qt::CaseInsensitive) || model.contains("vl", Qt::CaseInsensitive) || model.contains("qwen", Qt::CaseInsensitive)) {
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

void ComicProcessor::saveToDB(const ComicEntry &entry) {
    if (!QFile::exists(entry.imagePath)) {
        QMessageBox::warning(nullptr, "File Not Found", "Image file not found: " + entry.imagePath);
        return;
    }
    QSqlQuery query(db);
    if (!query.prepare("INSERT INTO comics (image_path, image_data, action, category, title, issue, publisher, year, genre, main_characters, condition, value, notes) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)")) {
        qDebug() << "Prepare failed:" << query.lastError();
        return;
    }
    query.addBindValue(entry.imagePath);
    // Read original image data to preserve resolution and format
    QFile file(entry.imagePath);
    QByteArray imageData;
    if (file.open(QIODevice::ReadOnly)) {
        imageData = file.readAll();
        file.close();
        qDebug() << "Original image data size:" << imageData.size();
    } else {
        QMessageBox::warning(nullptr, "File Error", "Failed to open image file: " + entry.imagePath);
        return;
    }
    query.addBindValue(imageData);
    query.addBindValue(entry.action);
    query.addBindValue(entry.category);
    query.addBindValue(entry.title);
    query.addBindValue(entry.issueNumber);
    query.addBindValue(entry.publisher);
    query.addBindValue(entry.publicationYear);
    query.addBindValue(entry.genre);
    query.addBindValue(entry.mainCharacters);
    query.addBindValue(entry.conditionID);
    query.addBindValue(entry.value);
    query.addBindValue(entry.notes);
    if (!query.exec()) {
        qDebug() << "Exec failed:" << query.lastError();
    } else {
        qDebug() << "DB insert success for" << entry.imagePath;
    }
}

ComicEntry ComicProcessor::parseResponse(const QString &response) {
    ComicEntry entry;
    entry.metadata = response;

    QStringList lines = response.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        if (line.startsWith("Title:", Qt::CaseInsensitive)) {
            entry.title = line.mid(6).trimmed();
        } else if (line.startsWith("Issue:", Qt::CaseInsensitive)) {
            entry.issueNumber = line.mid(6).trimmed();
        } else if (line.startsWith("Variant:", Qt::CaseInsensitive)) {
            QString variant = line.mid(8).trimmed();
            if (!variant.isEmpty() && !entry.issueNumber.isEmpty()) {
                entry.issueNumber += " " + variant;
            }
        } else if (line.startsWith("Publisher:", Qt::CaseInsensitive)) {
            entry.publisher = line.mid(10).trimmed();
        } else if (line.startsWith("Condition:", Qt::CaseInsensitive)) {
            entry.conditionID = line.mid(10).trimmed();
            entry.condition = entry.conditionID;
        } else if (line.startsWith("Value:", Qt::CaseInsensitive)) {
            entry.value = line.mid(6).trimmed();
        } else if (line.startsWith("Notes:", Qt::CaseInsensitive)) {
            entry.notes = line.mid(6).trimmed();
        }
    }

    // Set defaults if not found
    if (entry.title.isEmpty()) entry.title = "Unknown";
    if (entry.issueNumber.isEmpty()) entry.issueNumber = "Unknown";
    if (entry.publisher.isEmpty()) entry.publisher = "Unknown";
    if (entry.publicationYear.isEmpty()) entry.publicationYear = "Unknown";
    if (entry.genre.isEmpty()) entry.genre = "Superheroes";
    if (entry.conditionID.isEmpty()) entry.conditionID = "3000";
    if (entry.value.isEmpty()) entry.value = "";
    if (entry.notes.isEmpty()) entry.notes = response; // fallback

    // Series - assume title
    entry.series = entry.title;

    // Main Characters - not extracted
    entry.mainCharacters = "";

    // SKU
    entry.sku = QUuid::createUuid().toString();

    // eBay Title - use the AI generated title
    entry.ebay_title = entry.title;

    // eBay Description - use notes
    entry.ebay_description = entry.notes;

    // Price - leave empty
    entry.price = "";

    // Quantity
    entry.quantity = "1";

    // Category ID
    entry.category_id = "63";

    // Picture URL
    entry.picture_url = "";

    return entry;
}