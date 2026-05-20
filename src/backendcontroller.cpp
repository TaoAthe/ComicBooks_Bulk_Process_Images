#include "backendcontroller.h"

#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonArray>
#include <QJsonObject>
#include <QCoreApplication>
#include <QByteArray>
#include <QDateTime>
#include <QDebug>
#include <QMetaObject>
#include <QStandardPaths>
#include <QVariant>

BackendController::BackendController(QObject *parent)
    : QObject(parent),
      m_processing(false),
      m_generateEbay(false),
      m_processor(new ComicProcessor(this)),
      m_serverType(ComicProcessor::LMStudio) {
    QString configuredRoot;
    QByteArray envRoot = qgetenv("COMICVISION_ROOT");
    if (!envRoot.isEmpty()) {
        QFileInfo envInfo(QString::fromUtf8(envRoot));
        if (envInfo.exists() && envInfo.isDir()) {
            configuredRoot = envInfo.absoluteFilePath();
        }
    }

    if (configuredRoot.isEmpty()) {
        QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
        if (!pictures.isEmpty()) {
            QFileInfo picsInfo(pictures);
            if (picsInfo.exists() && picsInfo.isDir()) {
                configuredRoot = picsInfo.absoluteFilePath();
            }
        }
    }

    if (configuredRoot.isEmpty()) {
        configuredRoot = QDir(QCoreApplication::applicationDirPath()).filePath("images");
    }

    QString normalizedRoot = QDir::cleanPath(QFileInfo(configuredRoot).absoluteFilePath());
    QDir rootDir(normalizedRoot);
    if (!rootDir.exists()) {
        QDir().mkpath(normalizedRoot);
        rootDir = QDir(normalizedRoot);
    }
    m_rootPath = rootDir.absolutePath();
    m_currentPath = m_rootPath;
    ensureDatabaseReady();
}

void BackendController::ensureDatabaseReady() {
    m_db = QSqlDatabase::database();
    if (!m_db.isValid()) {
        m_db = QSqlDatabase::addDatabase("QSQLITE", "backend_controller_connection");
        m_db.setDatabaseName(QDir(QCoreApplication::applicationDirPath()).filePath("comicvision.db"));
    }
    if (!m_db.isOpen()) {
        if (!m_db.open()) {
            qWarning() << "Failed to open database" << m_db.lastError();
        }
    }
}

QString BackendController::rootPath() const { return m_rootPath; }

QString BackendController::currentPath() const { return m_currentPath; }

void BackendController::setCurrentPath(const QString &path) {
    QString normalized = normalizePath(path);
    if (!normalized.isEmpty()) {
        QString rootLower = QDir::cleanPath(m_rootPath).toLower();
        QString normalizedLower = QDir::cleanPath(normalized).toLower();
        if (!rootLower.isEmpty() && !normalizedLower.startsWith(rootLower)) {
            QString newRoot = deriveRootForPath(normalized);
            if (!newRoot.isEmpty()) {
                QDir rootDir(newRoot);
                if (rootDir.exists()) {
                    m_rootPath = rootDir.absolutePath();
                }
            }
        }
        m_currentPath = normalized;
    }
}

QString BackendController::normalizePath(const QString &path) const {
    if (path.isEmpty()) {
        return m_rootPath;
    }
    QString sanitized = QDir::fromNativeSeparators(path);
    QFileInfo info(sanitized);
    if (!info.exists()) {
        return {};
    }
    QString absolute = QDir::cleanPath(info.absoluteFilePath());
    return absolute;
}

QString BackendController::deriveRootForPath(const QString &absolutePath) const {
#ifdef Q_OS_WIN
    QFileInfo info(absolutePath);
    QString root = info.absoluteDir().rootPath();
    if (root.isEmpty()) {
        // root = info.rootPath();
    }
    if (root.isEmpty()) {
        root = absolutePath;
    }
    return QDir::cleanPath(root);
#else
    Q_UNUSED(absolutePath);
    return QStringLiteral("/");
#endif
}

bool BackendController::isSupportedImage(const QString &filePath) const {
    static const QStringList kSupported = {
        "jpg", "jpeg", "png", "gif", "bmp", "webp", "tiff", "tif"
    };
    return kSupported.contains(QFileInfo(filePath).suffix().toLower());
}

QJsonObject BackendController::directoryListing(const QString &requestedPath) const {
    QString path = requestedPath.isEmpty() ? m_currentPath : normalizePath(requestedPath);
    if (path.isEmpty()) {
        path = m_currentPath;
    }

    QDir dir(path);
    QJsonObject result;
    QString cleanedRoot = QDir::cleanPath(m_rootPath);
    QString cleanedCurrent = QDir::cleanPath(dir.absolutePath());
    result["path"] = cleanedCurrent;
    result["root"] = cleanedRoot;
#ifdef Q_OS_WIN
    bool isRoot = cleanedCurrent.compare(cleanedRoot, Qt::CaseInsensitive) == 0;
#else
    bool isRoot = cleanedCurrent == cleanedRoot;
#endif
    if (!isRoot) {
        QString parentPath = QFileInfo(cleanedCurrent).dir().absolutePath();
        result["parent"] = QDir::cleanPath(parentPath);
    }

    QJsonArray directories;
    for (const QFileInfo &entry : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QString entryPath = QDir::cleanPath(entry.absoluteFilePath());
        QJsonObject item;
        item["name"] = entry.fileName();
        item["path"] = entryPath;
        directories.append(item);
    }

    QJsonArray files;
    for (const QFileInfo &entry : dir.entryInfoList(QDir::Files)) {
        if (!isSupportedImage(entry.absoluteFilePath())) {
            continue;
        }
        QJsonObject item;
        item["name"] = entry.fileName();
        item["path"] = QDir::cleanPath(entry.absoluteFilePath());
        item["size"] = static_cast<qint64>(entry.size());
        item["lastModified"] = entry.lastModified().toString(Qt::ISODate);
        files.append(item);
    }

    result["directories"] = directories;
    result["files"] = files;
    return result;
}

namespace {
QStringList uniqueNormalized(const QStringList &paths) {
    QStringList normalized;
    for (const QString &path : paths) {
        QString cleaned = QDir::cleanPath(path);
        if (!normalized.contains(cleaned)) {
            normalized.append(cleaned);
        }
    }
    return normalized;
}
} // namespace

bool BackendController::addToQueue(const QStringList &paths, QStringList &errors) {
    QStringList uniquePaths = uniqueNormalized(paths);
    bool added = false;
    for (const QString &path : uniquePaths) {
        QFileInfo info(path);
        if (!info.exists() || !info.isFile()) {
            errors.append(QStringLiteral("File not found: %1").arg(path));
            continue;
        }
        if (!info.absoluteFilePath().startsWith(m_rootPath)) {
            errors.append(QStringLiteral("Path outside workspace: %1").arg(path));
            continue;
        }
        if (!isSupportedImage(path)) {
            errors.append(QStringLiteral("Unsupported image format: %1").arg(path));
            continue;
        }
        if (m_activeQueue.contains(info.absoluteFilePath()) || m_pendingQueue.contains(info.absoluteFilePath())) {
            continue;
        }
        m_activeQueue.append(info.absoluteFilePath());
        added = true;
    }
    if (added) {
        emit queueUpdated();
    }
    return added;
}

bool BackendController::removeFromQueue(const QString &path) {
    QString normalized = QDir::cleanPath(path);
    bool removed = m_activeQueue.removeAll(normalized) > 0;
    if (removed) {
        emit queueUpdated();
    }
    return removed;
}

void BackendController::clearQueue() {
    if (m_activeQueue.isEmpty()) {
        return;
    }
    m_activeQueue.clear();
    emit queueUpdated();
}

QJsonArray BackendController::queueState() const {
    QJsonArray array;
    for (const QString &item : m_activeQueue) {
        QJsonObject obj;
        obj["path"] = item;
        obj["name"] = QFileInfo(item).fileName();
        obj["status"] = "queued";
        array.append(obj);
    }
    bool firstPending = true;
    for (const QString &item : m_pendingQueue) {
        QJsonObject obj;
        obj["path"] = item;
        obj["name"] = QFileInfo(item).fileName();
        obj["status"] = firstPending ? "processing" : "pending";
        firstPending = false;
        array.append(obj);
    }
    return array;
}

QJsonObject BackendController::status() const {
    QJsonObject json;
    json["processing"] = m_processing;
    json["queueLength"] = static_cast<int>(m_activeQueue.size());
    json["pendingLength"] = m_pendingQueue.size();
    json["currentItem"] = m_currentItem;
    json["server"] = m_serverType == ComicProcessor::Ollama ? "Ollama" : "LMStudio";
    json["model"] = m_modelName;
    json["generateEbay"] = m_generateEbay;
    return json;
}

QJsonArray BackendController::processedComics() const {
    QJsonArray array;
    QSqlQuery query(m_db);
    if (!query.exec("SELECT id, title, issue, publisher, year, condition, value, notes FROM comics ORDER BY id DESC")) {
        qWarning() << "Failed to fetch comics" << query.lastError();
        return array;
    }
    while (query.next()) {
        QJsonObject obj;
        obj["id"] = query.value(0).toInt();
        obj["title"] = query.value(1).toString();
        obj["issue"] = query.value(2).toString();
        obj["publisher"] = query.value(3).toString();
        obj["year"] = query.value(4).toString();
        obj["condition"] = query.value(5).toString();
        obj["value"] = query.value(6).toString();
        obj["notes"] = query.value(7).toString();
        array.append(obj);
    }
    return array;
}

bool BackendController::deleteComic(int id) {
    if (id <= 0) {
        qWarning() << "Invalid comic id for deletion" << id;
        return false;
    }
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM comics WHERE id = ?");
    query.addBindValue(id);
    if (!query.exec()) {
        qWarning() << "Failed to delete comic" << id << query.lastError();
        return false;
    }
    return query.numRowsAffected() > 0;
}

bool BackendController::updateComic(int id, const QJsonObject &updates, QStringList &errors) {
    if (id <= 0) {
        errors.append(QStringLiteral("Invalid id"));
        return false;
    }
    if (updates.isEmpty()) {
        errors.append(QStringLiteral("No fields provided"));
        return false;
    }

    const QHash<QString, QString> allowedFields = {
        {QStringLiteral("title"), QStringLiteral("title")},
        {QStringLiteral("issue"), QStringLiteral("issue")},
        {QStringLiteral("publisher"), QStringLiteral("publisher")},
        {QStringLiteral("year"), QStringLiteral("year")},
        {QStringLiteral("condition"), QStringLiteral("condition")},
        {QStringLiteral("value"), QStringLiteral("value")},
        {QStringLiteral("notes"), QStringLiteral("notes")},
        {QStringLiteral("genre"), QStringLiteral("genre")},
        {QStringLiteral("main_characters"), QStringLiteral("main_characters")},
        {QStringLiteral("category"), QStringLiteral("category")},
        {QStringLiteral("action"), QStringLiteral("action")}
    };

    QStringList columns;
    QList<QVariant> values;
    for (auto it = updates.constBegin(); it != updates.constEnd(); ++it) {
        const QString &key = it.key();
        if (!allowedFields.contains(key)) {
            errors.append(QStringLiteral("Unsupported field: %1").arg(key));
            continue;
        }
        columns.append(allowedFields.value(key) + QLatin1String(" = ?"));
        values.append(it.value().toVariant());
    }

    if (columns.isEmpty()) {
        if (errors.isEmpty()) {
            errors.append(QStringLiteral("No valid fields supplied"));
        }
        return false;
    }

    QString statement = QStringLiteral("UPDATE comics SET %1 WHERE id = ?").arg(columns.join(QStringLiteral(", ")));
    QSqlQuery query(m_db);
    if (!query.prepare(statement)) {
        qWarning() << "Failed to prepare update" << query.lastError();
        errors.append(QStringLiteral("Database error preparing update"));
        return false;
    }
    for (const QVariant &value : values) {
        query.addBindValue(value);
    }
    query.addBindValue(id);

    if (!query.exec()) {
        qWarning() << "Failed to update comic" << id << query.lastError();
        errors.append(QStringLiteral("Database update failed"));
        return false;
    }

    if (query.numRowsAffected() == 0) {
        errors.append(QStringLiteral("Comic not found"));
        return false;
    }

    return true;
}

QJsonArray BackendController::gallery() const {
    QJsonArray array;
    QSqlQuery query(m_db);
    if (!query.exec("SELECT id, title, issue, publisher, year, condition, value FROM comics ORDER BY id DESC")) {
        return array;
    }
    while (query.next()) {
        QJsonObject item;
        item["id"] = query.value(0).toInt();
        item["title"] = query.value(1).toString();
        item["issue"] = query.value(2).toString();
        item["publisher"] = query.value(3).toString();
        item["year"] = query.value(4).toString();
        item["condition"] = query.value(5).toString();
        item["value"] = query.value(6).toString();
        array.append(item);
    }
    return array;
}

QByteArray BackendController::imageById(int id, QString &mimeType) const {
    QSqlQuery query(m_db);
    query.prepare("SELECT image_data FROM comics WHERE id = ?");
    query.addBindValue(id);
    if (!query.exec() || !query.next()) {
        mimeType.clear();
        return {};
    }
    QByteArray data = query.value(0).toByteArray();
    mimeType = "image/jpeg";
    return data;
}

void BackendController::configureProcessing(ComicProcessor::ServerType serverType,
                                            const QString &modelName,
                                            bool generateEbayListing) {
    m_serverType = serverType;
    m_modelName = modelName;
    m_generateEbay = generateEbayListing;
    m_processor->setServer(serverType);
    m_processor->setModel(modelName);
}

void BackendController::requestModels(ComicProcessor::ServerType serverType,
                                      std::function<void(QStringList)> callback) {
    if (m_cachedModels.contains(static_cast<int>(serverType))) {
        callback(m_cachedModels.value(static_cast<int>(serverType)));
        return;
    }
    m_processor->setServer(serverType);
    m_processor->fetchModels([this, serverType, callback](const QStringList &models) {
        m_cachedModels.insert(static_cast<int>(serverType), models);
        callback(models);
    });
}

void BackendController::startProcessing() {
    if (m_processing) {
        return;
    }
    if (m_activeQueue.isEmpty()) {
        return;
    }
    while (!m_activeQueue.isEmpty()) {
        m_pendingQueue.enqueue(m_activeQueue.takeFirst());
    }
    m_processing = true;
    emit statusUpdated();
    emit queueUpdated();
    processNext();
}

void BackendController::processNext() {
    if (m_pendingQueue.isEmpty()) {
        m_processing = false;
        m_currentItem.clear();
        emit statusUpdated();
        emit queueUpdated();
        return;
    }

    QString nextItem = m_pendingQueue.dequeue();
    m_currentItem = nextItem;
    emit statusUpdated();

    m_processor->processImage(nextItem, [this, nextItem](const QString &response) {
        QMetaObject::invokeMethod(this, [this, nextItem, response]() {
            if (response.startsWith("Error")) {
                qWarning() << "Processing failed for" << nextItem << response;
            }
            emit statusUpdated();
            processNext();
        }, Qt::QueuedConnection);
    });
}