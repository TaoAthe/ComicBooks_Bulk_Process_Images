#include "mainwindow.h"
#include "comicprocessor.h"
#include <QDir>
#include <QMessageBox>
#include <QLabel>
#include <QCoreApplication>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    // Set up GUI
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);

    serverCombo = new QComboBox(this);
    serverCombo->addItem("Ollama", ComicProcessor::Ollama);
    serverCombo->addItem("LM Studio", ComicProcessor::LMStudio);
    modelCombo = new QComboBox(this);
    modelInfoLabel = new QLabel("Model Info: Select a model", this);
    modelInfoLabel->setWordWrap(true);
    modelStatusLabel = new QLabel("Model Status: Not Ready", this);
    modelStatusLabel->setWordWrap(true);
    customPromptEdit = new QTextEdit(this);
    customPromptEdit->setPlaceholderText("Enter additional instructions for the AI here...");
    customPromptEdit->setMaximumHeight(60);

    processButton = new QPushButton("Process Images", this);
    processButton->setEnabled(false); // Initially disabled
    statusLabel = new QLabel("Status: Ready", this);
    statusLabel->setWordWrap(true);

    layout->addWidget(new QLabel("Server:", this));
    layout->addWidget(serverCombo);
    layout->addWidget(new QLabel("Model:", this));
    layout->addWidget(modelCombo);
    layout->addWidget(modelInfoLabel);
    layout->addWidget(modelStatusLabel);
    layout->addWidget(new QLabel("Additional Instructions:", this));
    layout->addWidget(customPromptEdit);
    layout->addWidget(processButton);
    layout->addWidget(statusLabel);

    connect(processButton, &QPushButton::clicked, this, &MainWindow::onProcessImages);
    connect(serverCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onServerChanged);
    connect(modelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onModelChanged);

    setCentralWidget(centralWidget);
    setWindowTitle("ComicVision Sorter - Stage 1");
    resize(400, 350);
    setMaximumWidth(1024);

    // Create directories if needed
    QDir().mkpath(QCoreApplication::applicationDirPath() + "/images");
    QDir().mkpath(QCoreApplication::applicationDirPath() + "/processed");

    processor = new ComicProcessor(this);

    // Initial fetch for default server
    onServerChanged(0);
}

MainWindow::~MainWindow() {}

void MainWindow::onServerChanged(int index) {
    ComicProcessor::ServerType server = (ComicProcessor::ServerType)serverCombo->itemData(index).toInt();
    processor->setServer(server);
    processor->fetchModels([this](const QStringList& models) {
        modelCombo->clear();
        modelCombo->addItems(models);
        if (!models.isEmpty()) {
            modelCombo->setCurrentIndex(0);
            onModelChanged(0); // Trigger info fetch
        } else {
            modelInfoLabel->setText("Model Info: No vision-capable models found");
            modelStatusLabel->setText("Model Status: Not Ready");
            processButton->setEnabled(false);
        }
    });
}

void MainWindow::onModelChanged(int index) {
    QString selectedModel = modelCombo->currentText();
    if (selectedModel.isEmpty()) {
        modelInfoLabel->setText("Model Info: Select a model");
        modelStatusLabel->setText("Model Status: Not Ready");
        processButton->setEnabled(false);
        return;
    }
    processor->fetchModelInfo(selectedModel, [this](const QString& info) {
        modelInfoLabel->setText("Model Info: " + info);
        if (info.contains("supports vision", Qt::CaseInsensitive)) {
            modelStatusLabel->setText("Model Status: Ready");
            processButton->setEnabled(true);
        } else {
            modelStatusLabel->setText("Model Status: Not Ready");
            processButton->setEnabled(false);
        }
    });
}

void MainWindow::onProcessImages() {
    statusLabel->setText("Status: Processing...");
    processButton->setEnabled(false);

    QStringList images = processor->scanImagesDirectory();
    if (images.isEmpty()) {
        statusLabel->setText("Status: No images found in " + QCoreApplication::applicationDirPath() + "/images");
        processButton->setEnabled(true);
        return;
    }

    // For Stage 1, process only the first image for testing
    QString imagePath = images.first();
    QString selectedModel = modelCombo->currentText();
    processor->setModel(selectedModel);
    processor->setCustomPrompt(customPromptEdit->toPlainText());
    processor->processImage(imagePath, [this](const QString& response) {
        statusLabel->setText(QString("Status: AI Response: %1").arg(response));
        qDebug() << "Full AI Response:" << response;
        if (response.startsWith("Error")) {
            QMessageBox::information(this, "Processing Error", response);
        }
        processButton->setEnabled(true);
    });
}