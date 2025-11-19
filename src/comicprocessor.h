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

class ComicProcessor : public QObject {
    Q_OBJECT

public:
    enum ServerType { Ollama, LMStudio };

    explicit ComicProcessor(QObject *parent = nullptr);
    QStringList scanImagesDirectory();
    void processImage(const QString &imagePath, std::function<void(const QString&)> callback);
    void setServer(ServerType s);
    void setModel(const QString &m);
    void setCustomPrompt(const QString &p);
    void fetchModels(std::function<void(const QStringList&)> callback);
    void fetchModelInfo(const QString &model, std::function<void(const QString&)> callback);
    void saveToDB(const QString &imagePath, const QString &metadata);

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
};

#endif // COMICPROCESSOR_H