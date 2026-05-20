#ifndef BACKENDCONTROLLER_H
#define BACKENDCONTROLLER_H

#include <QObject>
#include <QQueue>
#include <QJsonObject>
#include <QJsonArray>
#include <QHash>
#include <QStringList>
#include <QSqlDatabase>
#include <functional>
#include "comicprocessor.h"

class BackendController : public QObject {
    Q_OBJECT

public:
    explicit BackendController(QObject *parent = nullptr);

    QJsonObject directoryListing(const QString &requestedPath) const;
    QJsonArray queueState() const;
    QJsonObject status() const;
    QJsonArray processedComics() const;
    QJsonArray gallery() const;
    QByteArray imageById(int id, QString &mimeType) const;
    bool deleteComic(int id);
    bool updateComic(int id, const QJsonObject &updates, QStringList &errors);

    QString rootPath() const;
    QString currentPath() const;
    void setCurrentPath(const QString &path);

    bool addToQueue(const QStringList &paths, QStringList &errors);
    bool removeFromQueue(const QString &path);
    void clearQueue();

    void configureProcessing(ComicProcessor::ServerType serverType,
                             const QString &modelName,
                             bool generateEbayListing,
                             const QString &apiKey = {});
    void requestModels(ComicProcessor::ServerType serverType,
                       const QString &apiKey,
                       std::function<void(QStringList)> callback);

public slots:
    void startProcessing();

signals:
    void queueUpdated();
    void statusUpdated();

private:
    void ensureDatabaseReady();
    void processNext();

    QString normalizePath(const QString &path) const;
    QString deriveRootForPath(const QString &absolutePath) const;
    bool isSupportedImage(const QString &filePath) const;

    QString m_rootPath;
    QString m_currentPath;
    QQueue<QString> m_pendingQueue;
    QStringList m_activeQueue;
    bool m_processing;
    bool m_generateEbay;
    QString m_currentItem;

    ComicProcessor *m_processor;
    ComicProcessor::ServerType m_serverType;
    QString m_modelName;
    QString m_apiKey;

    mutable QSqlDatabase m_db;
    QHash<int, QStringList> m_cachedModels;
};

#endif // BACKENDCONTROLLER_H