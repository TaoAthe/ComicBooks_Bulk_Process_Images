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
#include <QUuid>
#include <QProcess>
#include <QTemporaryFile>
#include <QRegularExpression>

ComicProcessor::ComicProcessor(QObject *parent) : QObject(parent) {
    networkManager = new QNetworkAccessManager(this);
    server = LMStudio; // default
    model = ""; // default

    // Ensure directories exist
    QDir().mkpath(QCoreApplication::applicationDirPath() + "/images");
    QDir().mkpath(QCoreApplication::applicationDirPath() + "/processed");

    db = QSqlDatabase::addDatabase("QSQLITE", "main_db");
    db.setDatabaseName(QCoreApplication::applicationDirPath() + "/comicvision.db");
    if (!db.open()) {
        qDebug() << "Failed to open database";
    } else {
        QSqlQuery query(db);
        query.exec("CREATE TABLE IF NOT EXISTS comics (id INTEGER PRIMARY KEY AUTOINCREMENT, image_path TEXT, image_data BLOB, action TEXT, category TEXT, title TEXT, issue TEXT, publisher TEXT, year TEXT, genre TEXT, main_characters TEXT, condition TEXT, value TEXT, notes TEXT)");
        qDebug() << "Create comics table result:" << query.lastError().text();
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

    // Open GCD database (read-only enrichment)
    QString gcdPath = QCoreApplication::applicationDirPath() + "/gcd.db";
    if (QFile::exists(gcdPath)) {
        gcdDb = QSqlDatabase::addDatabase("QSQLITE", "gcd_db");
        gcdDb.setDatabaseName(gcdPath);
        gcdDb.setConnectOptions("QSQLITE_OPEN_READONLY");
        if (!gcdDb.open()) {
            qDebug() << "GCD database failed to open:" << gcdDb.lastError().text();
        } else {
            qDebug() << "GCD database opened:" << gcdPath;
        }
    } else {
        qDebug() << "GCD database not found at" << gcdPath << "— skipping enrichment";
    }
}

ComicProcessor::~ComicProcessor() {
    // Abort all pending network requests
    QList<QNetworkReply*> replies = findChildren<QNetworkReply*>();
    for (QNetworkReply *reply : replies) {
        reply->abort();
    }
    if (db.isOpen()) db.close();
    if (gcdDb.isOpen()) gcdDb.close();
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
    } else if (server == Gemini) {
        return "https://generativelanguage.googleapis.com/v1beta/openai";
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
        entry.picture_url = fullPath;        lookupGCD(entry);        saveToDB(entry);
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
    QUrl url(getServerUrl() + (server == Ollama ? "/api/chat" : "/v1/chat/completions"));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    // LM Studio's local server doesn't support HTTP/2 multiplexing.
    // Force HTTP/1.1 to avoid "unknown channel" warnings.
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    // Gemini requires a Bearer token; injected per-request so it is never stored.
    if (server == Gemini && !m_apiKey.isEmpty()) {
        request.setRawHeader("Authorization",
            QStringLiteral("Bearer %1").arg(m_apiKey).toUtf8());
    }

    QJsonObject json;
    json["model"] = model;
    QString prompt =
        "You are analyzing a comic book cover image for eBay resale. "
        "Respond with ONLY the following labeled fields — no markdown, no extra commentary, no <think> blocks.\n\n"
        "CRITICAL: The Title field must be the FULL series name exactly as it appears printed on the cover — "
        "including any qualifiers like 'Peter Parker, The Spectacular' or 'The Amazing' or 'Web of'. "
        "Do NOT abbreviate to just the character name. "
        "Examples: 'Peter Parker, The Spectacular Spider-Man' not 'Spider-Man'; "
        "'The Amazing Spider-Man' not 'Spider-Man'; "
        "'Marvel Feature Presents Red Sonja' not 'Red Sonja'.\n\n"
        "Title: [full series name exactly as printed, including all qualifiers]\n"
        "Issue: [issue number, digits only]\n"
        "Year: [cover year if visible, otherwise estimate from style/price/Comics Code logo]\n"
        "Publisher: [publisher name]\n"
        "Condition: [one of: Poor, Fair, Good, Very Good, Fine, Very Fine, Near Mint]\n"
        "Value: [estimated resale value in USD, e.g. $12.00]\n"
        "eBay Title: [optimized eBay listing title max 80 chars, e.g. 'Marvel Peter Parker Spectacular Spider-Man #27 1978 VF']\n"
        "Notes: [1-2 sentences describing visible condition defects, key characters, and story arc]";
    if (!customPrompt.isEmpty()) {
        prompt += "\n" + customPrompt;
    }

    if (server == Ollama) {
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
        json["stream"] = false;
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
                    // /api/chat response: {"message": {"role": "assistant", "content": "..."}}
                    response = doc.object()["message"].toObject()["content"].toString();
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

void ComicProcessor::setApiKey(const QString &key) {
    m_apiKey = key;
}

void ComicProcessor::fetchModels(std::function<void(const QStringList&)> callback) {
    QString endpoint = (server == Ollama) ? "/api/tags" : "/v1/models";
    QUrl url(getServerUrl() + endpoint);
    QNetworkRequest request(url);
    if ((server == Gemini) && !m_apiKey.isEmpty()) {
        request.setRawHeader("Authorization",
            QStringLiteral("Bearer %1").arg(m_apiKey).toUtf8());
    }

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
    // For Gemini, only surface models that contain "gemini" in their id.
    if (server == Gemini) {
        QStringList filtered;
        for (const QString &m : allModels) {
            if (m.contains(QLatin1String("gemini"), Qt::CaseInsensitive)) {
                filtered << m;
            }
        }
        callback(filtered.isEmpty() ? allModels : filtered);
        return;
    }
    // Return all models for local servers — let the user pick.
    callback(allModels);
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
        qWarning() << "Image file not found:" << entry.imagePath;
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
        qWarning() << "Failed to open image file:" << entry.imagePath;
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

static QString stripMarkdown(const QString &s) {
    QString r = s;
    r.remove(QRegularExpression("[*_`]+"));
    return r.trimmed();
}

static QString conditionTextToEbayCode(const QString &text) {
    QString t = text.toLower().trimmed();
    if (t.contains("near mint") || t.contains("nm"))  return "4000";
    if (t.contains("very fine") || t.contains("vf"))  return "4000";
    if (t.contains("fine") || t == "fn")              return "3000";
    if (t.contains("very good") || t.contains("vg"))  return "2750";
    if (t.contains("good") || t == "g")               return "2500";
    if (t.contains("fair"))                           return "2000";
    if (t.contains("poor"))                           return "2000";
    // Already a numeric code?
    if (t == "4000" || t == "3000" || t == "2750" || t == "2500" || t == "2000") return text.trimmed();
    return "3000"; // default Very Good
}

ComicEntry ComicProcessor::parseResponse(const QString &response) {
    ComicEntry entry;
    entry.metadata = response;

    // Strip <think>...</think> reasoning blocks produced by some models
    QString cleaned = response;
    QRegularExpression thinkRe("<think>.*?</think>", QRegularExpression::DotMatchesEverythingOption);
    cleaned.remove(thinkRe);
    cleaned = cleaned.trimmed();

    QStringList lines = cleaned.split('\n', Qt::SkipEmptyParts);
    for (const QString &rawLine : lines) {
        // Strip leading markdown bold/italic markers from the whole line
        QString line = rawLine;
        line.remove(QRegularExpression("^[*_`#]+\\s*"));
        line = line.trimmed();

        if (line.startsWith("Title:", Qt::CaseInsensitive)) {
            entry.title = stripMarkdown(line.mid(6));
        } else if (line.startsWith("Issue:", Qt::CaseInsensitive)) {
            entry.issueNumber = stripMarkdown(line.mid(6));
        } else if (line.startsWith("Year:", Qt::CaseInsensitive)) {
            entry.publicationYear = stripMarkdown(line.mid(5));
        } else if (line.startsWith("Variant:", Qt::CaseInsensitive)) {
            QString variant = stripMarkdown(line.mid(8));
            if (!variant.isEmpty() && !entry.issueNumber.isEmpty()) {
                entry.issueNumber += " " + variant;
            }
        } else if (line.startsWith("Publisher:", Qt::CaseInsensitive)) {
            entry.publisher = stripMarkdown(line.mid(10));
        } else if (line.startsWith("eBay Title:", Qt::CaseInsensitive)) {
            entry.ebay_title = stripMarkdown(line.mid(11));
        } else if (line.startsWith("Condition:", Qt::CaseInsensitive)) {
            QString condText = stripMarkdown(line.mid(10));
            entry.condition = condText;
            entry.conditionID = conditionTextToEbayCode(condText);
        } else if (line.startsWith("Value:", Qt::CaseInsensitive)) {
            entry.value = stripMarkdown(line.mid(6));
        } else if (line.startsWith("Notes:", Qt::CaseInsensitive)) {
            entry.notes = stripMarkdown(line.mid(6));
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

    // Series - full exact title is the catalogue series identifier
    entry.series = entry.title;

    // Main Characters - not extracted
    entry.mainCharacters = "";

    // SKU
    entry.sku = QUuid::createUuid().toString();

    // eBay Title - use AI-generated if present, fall back to title
    if (entry.ebay_title.isEmpty()) entry.ebay_title = entry.title;

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

void ComicProcessor::lookupGCD(ComicEntry &entry) {
    if (!gcdDb.isOpen()) return;
    if (entry.title.isEmpty() || entry.title == "Unknown") return;
    if (entry.issueNumber.isEmpty() || entry.issueNumber == "Unknown") return;

    // Collect candidate series — exact match first, then fuzzy anchor word
    struct Candidate { int id; QString name; int yearBegan; QString publisher; };
    QVector<Candidate> candidates;

    auto runSeriesQuery = [&](const QString &sql, const QString &param) {
        QSqlQuery q(gcdDb);
        q.prepare(sql);
        q.addBindValue(param);
        if (!q.exec()) {
            qDebug() << "GCD series query error:" << q.lastError().text();
            return;
        }
        while (q.next()) {
            candidates.append({q.value(0).toInt(),
                                q.value(1).toString(),
                                q.value(2).toInt(),
                                q.value(3).toString()});
        }
    };

    const QString seriesSql =
        "SELECT s.id, s.name, s.year_began, COALESCE(p.name,'') "
        "FROM series s LEFT JOIN publisher p ON p.id=s.publisher_id "
        "WHERE LOWER(s.name)=LOWER(?) LIMIT 10";

    runSeriesQuery(seriesSql, entry.title);

    if (candidates.isEmpty()) {
        // Pick the longest word (>=5 chars) as anchor for fuzzy search
        QString anchor;
        for (const QString &w : entry.title.split(' ', Qt::SkipEmptyParts)) {
            if (w.length() >= 5 && w.length() > anchor.length()) anchor = w;
        }
        if (!anchor.isEmpty()) {
            const QString fuzzySql =
                "SELECT s.id, s.name, s.year_began, COALESCE(p.name,'') "
                "FROM series s LEFT JOIN publisher p ON p.id=s.publisher_id "
                "WHERE LOWER(s.name) LIKE LOWER(?) LIMIT 30";
            runSeriesQuery(fuzzySql, "%" + anchor + "%");
        }
    }

    if (candidates.isEmpty()) return;

    // For each candidate, verify the issue number exists and score the match
    int bestId = -1;
    QString bestName, bestPublisher;
    int bestYear = 0, bestScore = -1;

    int aiYear = 0;
    if (!entry.publicationYear.isEmpty() && entry.publicationYear != "Unknown")
        aiYear = entry.publicationYear.left(4).toInt();

    for (const auto &c : candidates) {
        QSqlQuery iq(gcdDb);
        iq.prepare("SELECT COUNT(*) FROM issue WHERE series_id=? AND number=?");
        iq.addBindValue(c.id);
        iq.addBindValue(entry.issueNumber);
        if (!iq.exec() || !iq.next() || iq.value(0).toInt() == 0)
            continue;   // issue not found in this series

        int score = 10; // base: issue confirmed

        // Publisher match (compare first 5 chars, case-insensitive)
        if (!c.publisher.isEmpty() && !entry.publisher.isEmpty()) {
            if (c.publisher.toLower().contains(entry.publisher.toLower().left(5).toLower()))
                score += 5;
        }

        // Year proximity
        if (aiYear > 0 && c.yearBegan > 0) {
            int diff = qAbs(aiYear - c.yearBegan);
            if (diff <= 2)  score += 4;
            else if (diff <= 5) score += 2;
        }

        if (score > bestScore) {
            bestScore = score;
            bestId    = c.id;
            bestName  = c.name;
            bestYear  = c.yearBegan;
            bestPublisher = c.publisher;
        }
    }

    if (bestId < 0) {
        qDebug() << "GCD: no confirmed match for" << entry.title << "#" << entry.issueNumber;
        return;
    }

    qDebug() << "GCD match:" << bestName << "#" << entry.issueNumber
             << "| pub:" << bestPublisher << "| year_began:" << bestYear
             << "| score:" << bestScore;

    // Enrich entry with verified GCD data
    entry.series = bestName;   // canonical GCD series name
    if (!bestPublisher.isEmpty())
        entry.publisher = bestPublisher;
    if (bestYear > 0 && (entry.publicationYear.isEmpty() || entry.publicationYear == "Unknown"))
        entry.publicationYear = QString::number(bestYear);
}