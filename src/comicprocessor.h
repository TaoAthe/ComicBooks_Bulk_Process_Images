#ifndef COMICPROCESSOR_H
#define COMICPROCESSOR_H

#include <QObject>
#include <QNetworkAccessManager>
#include <functional>
#include <memory>
#include <QCoreApplication>
#include <QImageReader>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QRegularExpression>

struct ComicEntry {
    QString action = "Add";
    QString category = "180089";
    QString title;
    QString conditionID = "3000";
    QString publisher;
    QString series;
    QString issueNumber;
    QString publicationYear;
    QString genre = "Superheroes";
    QString mainCharacters;
    QString description;
    // internal metadata...
    QString sku;
    QString ebay_title;
    QString ebay_description;
    QString price;
    QString quantity = "1";
    QString condition = "3000";
    QString category_id = "63";
    QString picture_url;
    QString imagePath;
    QString metadata;
    QString value;
    QString notes;
};

class ComicProcessor : public QObject {
    Q_OBJECT

public:
    enum ServerType { Ollama, LMStudio };

    explicit ComicProcessor(QObject *parent = nullptr);
    ~ComicProcessor();
    QStringList scanImagesDirectory();
    void processImage(const QString &imagePath, std::function<void(const QString&)> callback);
    void setServer(ServerType s);
    void setModel(const QString &m);
    void setCustomPrompt(const QString &p);
    void fetchModels(std::function<void(const QStringList&)> callback);
    void fetchModelInfo(const QString &model, std::function<void(const QString&)> callback);
    void saveToDB(const QString &imagePath, const QString &metadata);
    void saveToDB(const ComicEntry &entry);
    void abortRequests();

private:
    QNetworkAccessManager *networkManager;
    ServerType server;
    QString model;
    QString customPrompt;
    QSqlDatabase db;
    QString base64EncodeImage(const QString &imagePath);
    void sendToAI(const QString &base64Image, std::function<void(const QString&)> callback);
    void moveToProcessed(const QString &imagePath);
    QString getServerUrl() const;
    void filterVisionModels(const QStringList &allModels, std::function<void(const QStringList&)> callback);
    ComicEntry parseResponse(const QString &response);
};

#endif // COMICPROCESSOR_H