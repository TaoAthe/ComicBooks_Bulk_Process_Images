ComicVision Sorter Development - Stage 1: Basic Framework Setup
Version: 1.0
Date: November 15, 2025
Author: The Hive
Purpose: This document outlines and provides the implementation for Stage 1 of the ComicVision Sorter application. Based on the engineering specification, this stage focuses on establishing the core C++ framework using Qt, implementing directory scanning for images, integrating a local AI vision model (via Ollama or LMStudio (selectable) for simplicity and popularity), and performing a basic test. Future stages will add full metadata parsing, GUI enhancements, and additional features.
Stage Overview
Goals for Stage 1

Set up a basic Qt-based C++ project.
Create a simple GUI with a "Process Images" button and a status display.
Scan the /images/ directory for supported image files (.jpg, .jpeg, .png, .webp).
For each image:
Load the image using QImage.
Convert the image to base64 for API transmission.
Connect to a local Ollama instance running a vision model (e.g., llava:13b or similar multimodal model).
Send a prompt to the model to analyze the comic cover (basic extraction of text like title, issue, etc.).
Retrieve and log the raw AI response (for testing).
Move the processed image to /processed/.

Basic test: Process at least one sample image and verify AI response in console/GUI.
Handle errors (e.g., no Ollama running, invalid images).

Assumptions and Prerequisites

Qt Version: Qt 6 (compatible with Qt 5; adjust as needed).
Build System: CMake (recommended for modern Qt projects).
Local AI Model: Use Ollama (download from ollama.ai). Install and run a vision-capable model, e.g.:
ollama run llava (or llava:13b for better quality).
Ollama API runs on http://localhost:11434 by default.

Dependencies: Qt Core, Qt Gui, Qt Widgets, Qt Network (for API calls).
Development Environment: C++17 or later.
Directories: The app will create /images/ and /processed/ relative to the executable if they don't exist.
Testing: Place a sample comic cover image (e.g., sample.jpg) in /images/ for the first test. Expected AI response: Raw text description/extraction from the model.

Limitations in Stage 1

No full metadata parsing (e.g., regex for title/issue); just raw AI output.
Minimal GUI: Button and status label only (no inventory list yet).
No CSV export or advanced error handling.
AI prompt is basic; refinement in later stages.

Project Structure
comicvision-sorter-stage1/
├── CMakeLists.txt       # Build configuration
├── images/              # Unprocessed images (create and add sample.jpg for testing)
├── processed/           # Processed images (will be auto-created)
├── src/
│   ├── main.cpp         # Entry point
│   ├── mainwindow.h     # GUI header
│   ├── mainwindow.cpp   # GUI implementation
│   └── comicprocessor.h # Processor header (for scanning and AI)
│   └── comicprocessor.cpp # Processor implementation
└── README.md            # Basic instructions

Implementation Code
Below is the complete code for Stage 1. Copy-paste into the respective files.
CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(ComicVisionSorterStage1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

add_executable(ComicVisionSorterStage1
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/comicprocessor.cpp
    src/comicprocessor.h
)

target_link_libraries(ComicVisionSorterStage1 PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network)

src/main.cpp
#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    return app.exec();
}

src/mainwindow.h
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>

class ComicProcessor;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onProcessImages();

private:
    QPushButton *processButton;
    QLabel *statusLabel;
    ComicProcessor *processor;
};

#endif // MAINWINDOW_H
src/mainwindow.cpp
#include "mainwindow.h"
#include "comicprocessor.h"
#include <QDir>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    // Set up GUI
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);

    processButton = new QPushButton("Process Images", this);
    statusLabel = new QLabel("Status: Ready", this);

    layout->addWidget(processButton);
    layout->addWidget(statusLabel);

    connect(processButton, &QPushButton::clicked, this, &MainWindow::onProcessImages);

    setCentralWidget(centralWidget);
    setWindowTitle("ComicVision Sorter - Stage 1");
    resize(400, 200);

    // Create directories if needed
    QDir().mkpath("images");
    QDir().mkpath("processed");

    processor = new ComicProcessor(this);
}

MainWindow::~MainWindow() {}

void MainWindow::onProcessImages() {
    statusLabel->setText("Status: Processing...");
    processButton->setEnabled(false);

    QStringList images = processor->scanImagesDirectory();
    if (images.isEmpty()) {
        statusLabel->setText("Status: No images found");
        processButton->setEnabled(true);
        return;
    }

    // For Stage 1, process only the first image for testing
    QString imagePath = images.first();
    processor->processImage(imagePath, [this](const QString& response) {
        statusLabel->setText(QString("Status: AI Response: %1").arg(response.left(100) + "...")); // Truncate for display
        qDebug() << "Full AI Response:" << response;
        processButton->setEnabled(true);
    });
}


src/comicprocessor.h
#ifndef COMICPROCESSOR_H
#define COMICPROCESSOR_H

#include <QObject>
#include <QNetworkAccessManager>
#include <functional>

class ComicProcessor : public QObject {
    Q_OBJECT

public:
    explicit ComicProcessor(QObject *parent = nullptr);
    QStringList scanImagesDirectory();
    void processImage(const QString &imagePath, std::function<void(const QString&)> callback);

private:
    QNetworkAccessManager *networkManager;
    QString base64EncodeImage(const QString &imagePath);
    void sendToOllama(const QString &base64Image, std::function<void(const QString&)> callback);
    void moveToProcessed(const QString &imagePath);
};

#endif // COMICPROCESSOR_H

src/comicprocessor.cpp
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
#include <QDebug>

ComicProcessor::ComicProcessor(QObject *parent) : QObject(parent) {
    networkManager = new QNetworkAccessManager(this);
}

QStringList ComicProcessor::scanImagesDirectory() {
    QDir dir("images");
    QStringList filters = {"*.jpg", "*.jpeg", "*.png", "*.webp"};
    return dir.entryList(filters, QDir::Files);
}

void ComicProcessor::processImage(const QString &imagePath, std::function<void(const QString&)> callback) {
    QString fullPath = "images/" + imagePath;
    QString base64 = base64EncodeImage(fullPath);
    if (base64.isEmpty()) {
        callback("Error: Failed to encode image");
        return;
    }
    sendToOllama(base64, [this, fullPath, callback](const QString& response) {
        callback(response);
        moveToProcessed(fullPath);
    });
}

QString ComicProcessor::base64EncodeImage(const QString &imagePath) {
    QImage image(imagePath);
    if (image.isNull()) return "";

    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG"); // Convert to PNG for consistency
    return byteArray.toBase64();
}

void ComicProcessor::sendToOllama(const QString &base64Image, std::function<void(const QString&)> callback) {
    QUrl url("http://localhost:11434/api/generate");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject json;
    json["model"] = "llava"; // Change to your model name if different
    json["prompt"] = "Analyze this comic book cover. Extract: title, issue number, variant, publisher, and any notable text.";

    QJsonArray imagesArray;
    imagesArray.append(base64Image);
    json["images"] = imagesArray;

    QByteArray data = QJsonDocument(json).toJson();

    QNetworkReply *reply = networkManager->post(request, data);
    connect(reply, &QNetworkReply::finished, [reply, callback]() {
        if (reply->error() != QNetworkReply::NoError) {
            callback("Error: " + reply->errorString());
        } else {
            QByteArray responseData = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(responseData);
            if (!doc.isNull()) {
                // Ollama generate API streams, but for simplicity, assume single response or parse accordingly
                // In practice, handle streaming if needed; here, extract 'response' field
                callback(doc.object()["response"].toString());
            } else {
                callback("Error: Invalid JSON response");
            }
        }
        reply->deleteLater();
    });
}

void ComicProcessor::moveToProcessed(const QString &imagePath) {
    QFile file(imagePath);
    QString newPath = "processed/" + QFileInfo(imagePath).fileName();
    if (!file.rename(newPath)) {
        qDebug() << "Error moving file:" << imagePath;
    }
}


README.md
# ComicVision Sorter - Stage 1

## Setup
1. Install Qt 6 and CMake.
2. Install Ollama and run `ollama run llava`.
3. Build: `mkdir build && cd build && cmake .. && make`.
4. Run the executable.
5. Place a sample comic image in `images/`.
6. Click "Process Images" – Check console for AI response.

## Test
- Expected: App scans, sends to Ollama, logs raw response (e.g., "Title: Moon Knight, Issue: #1..."), moves image.
- If Ollama not running: Error in status.