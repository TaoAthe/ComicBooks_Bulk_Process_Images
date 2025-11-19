#include "mainwindow.h"
#include "comicprocessor.h"
#include "imageviewerwidget.h"
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QLabel>
#include <QCoreApplication>
#include <QCheckBox>
#include <QHeaderView>
#include <QPixmap>
#include <QImageReader>
#include <QTableWidgetItem>
#include <QSqlQuery>
#include <QSqlError>
#include <QMainWindow>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QComboBox>
#include <QTextEdit>
#include <QProcess>
#include <QTemporaryFile>
#include <QPushButton>
#include <QCloseEvent>
#include <QJsonDocument>
#include <QJsonObject>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    // Set up GUI
    tabWidget = new QTabWidget(this);
    tabWidget->setTabsClosable(true);
    connect(tabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTab);
    QWidget *mainTab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(mainTab);

    serverCombo = new QComboBox(this);
    serverCombo->addItem("Ollama", ComicProcessor::Ollama);
    serverCombo->addItem("LM Studio", ComicProcessor::LMStudio);
    serverCombo->setCurrentIndex(1); // Default to LM Studio
    modelCombo = new QComboBox(this);
    modelInfoLabel = new QLabel("Model Info: Select a model", this);
    modelInfoLabel->setWordWrap(true);
    modelStatusLabel = new QLabel("Model Status: Not Ready", this);
    modelStatusLabel->setWordWrap(true);
    // customPromptEdit = new QTextEdit(this);
    // customPromptEdit->setPlaceholderText("Enter additional instructions for the AI here...");
    // customPromptEdit->setMaximumHeight(60);

    ebayCheckBox = new QCheckBox("Generate eBay Listing", this);

    table = new QTableWidget(this);
    table->setColumnCount(14); // Delete + Thumbnail + 11 columns + View Image
    QStringList headers = {"", "Thumbnail", "Action", "Category", "Title", "Issue", "Publisher", "Year", "Genre", "Main Characters", "Condition", "Value", "Notes", "View Image"};
    table->setHorizontalHeaderLabels(headers);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    // table->setSortingEnabled(true);
    table->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    processButton = new QPushButton("Process Images", this);
    processButton->setEnabled(false); // Initially disabled

    pauseButton = new QPushButton("Pause", this);
    pauseButton->setEnabled(false); // Initially disabled

    layout->addWidget(new QLabel("Server:", this));
    layout->addWidget(serverCombo);
    layout->addWidget(new QLabel("Model:", this));
    layout->addWidget(modelCombo);
    layout->addWidget(modelInfoLabel);
    layout->addWidget(modelStatusLabel);
    // layout->addWidget(new QLabel("Additional Instructions:", this));
    // layout->addWidget(customPromptEdit);
    layout->addWidget(ebayCheckBox);
    layout->addWidget(table);
    layout->addWidget(processButton);
    layout->addWidget(pauseButton);

    connect(processButton, &QPushButton::clicked, this, &MainWindow::onProcessImages);
    connect(pauseButton, &QPushButton::clicked, this, &MainWindow::onPause);
    connect(serverCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onServerChanged);
    connect(modelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onModelChanged);
    connect(table, &QTableWidget::cellChanged, this, &MainWindow::onCellChanged);

    tabWidget->addTab(mainTab, "Main");
    setCentralWidget(tabWidget);
    setWindowTitle("ComicVision Sorter - Stage 1");
    resize(1200, 600);

    paused = false;

    statusBar()->showMessage("Ready");

    // Create directories if needed
    QDir().mkpath(QCoreApplication::applicationDirPath() + "/images");
    QDir().mkpath(QCoreApplication::applicationDirPath() + "/processed");

    processor = new ComicProcessor(this);

    qDebug() << "Supported image formats:" << QImageReader::supportedImageFormats();

    // Get the database from ComicProcessor
    db = QSqlDatabase::database();

    // Load existing data
    loadData();

    // Initial fetch for default server
    onServerChanged(0);
}

MainWindow::~MainWindow() { if (db.isOpen()) db.close(); }

void MainWindow::closeEvent(QCloseEvent *event) {
    processor->abortRequests();
    event->accept();
}

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
    statusBar()->showMessage("Processing images...");
    processButton->setEnabled(false);
    pauseButton->setEnabled(true);
    paused = false;
    pauseButton->setText("Pause");

    QStringList images = processor->scanImagesDirectory();
    if (images.isEmpty()) {
        statusBar()->showMessage("No images found in images directory");
        processButton->setEnabled(true);
        pauseButton->setEnabled(false);
        return;
    }

    pendingImages = images;
    QString selectedModel = modelCombo->currentText();
    if (selectedModel.isEmpty()) {
        QMessageBox::warning(this, "No Model Selected", "Please select an AI model before processing images.");
        statusBar()->showMessage("Ready");
        processButton->setEnabled(true);
        pauseButton->setEnabled(false);
        return;
    }
    processor->setModel(selectedModel);
    processNext();
}

void MainWindow::processNext() {
    if (pendingImages.isEmpty() || paused) {
        if (pendingImages.isEmpty()) {
            loadData();
            statusBar()->showMessage("Processing complete");
            processButton->setEnabled(true);
            pauseButton->setEnabled(false);
        }
        return;
    }
    QString imagePath = pendingImages.takeFirst();
    processor->processImage(imagePath, [this, imagePath](const QString& response) {
        qDebug() << "Full AI Response:" << response;
        if (response.startsWith("Error")) {
            QMessageBox::information(this, "Processing Error", response);
        }
        processNext();
    });
}

void MainWindow::loadData() {
    statusBar()->showMessage("Loading data...");
    table->setRowCount(0);
    QSqlQuery query("SELECT id, image_data, action, category, title, issue, publisher, year, genre, main_characters, condition, value, notes FROM comics");
    int row = 0;
    while (query.next()) {
        table->insertRow(row);
        int id = query.value(0).toInt();
        QByteArray imageData = query.value(1).toByteArray();
        // Delete button
        QPushButton *deleteBtn = new QPushButton("X");
        deleteBtn->setMaximumWidth(30);
        connect(deleteBtn, &QPushButton::clicked, [this, id]() {
            QSqlQuery query(db);
            query.prepare("DELETE FROM comics WHERE id = ?");
            query.addBindValue(id);
            if (query.exec()) {
                loadData();
            } else {
                qDebug() << "Delete failed for id" << id << query.lastError();
            }
        });
        table->setCellWidget(row, 0, deleteBtn);
        // Thumbnail
        QLabel *thumbnailLabel = new QLabel();
        QPixmap pixmap;
        QByteArray displayData = imageData;
        bool loaded = !imageData.isEmpty() && pixmap.loadFromData(imageData);
        if (!loaded && imageData.size() > 12 && imageData.startsWith("RIFF") && imageData.mid(8, 4) == "WEBP") {
            // Convert WebP to PNG for display
            QTemporaryFile tempWebp;
            if (tempWebp.open()) {
                tempWebp.write(imageData);
                tempWebp.close();
                QString webpPath = tempWebp.fileName();
                QTemporaryFile tempPng;
                tempPng.setFileTemplate(QDir::tempPath() + "/thumb_XXXXXX.png");
                if (tempPng.open()) {
                    QString pngPath = tempPng.fileName();
                    tempPng.close();
                    QProcess process;
                    process.start("magick", QStringList() << "convert" << webpPath << pngPath);
                    if (process.waitForFinished(10000)) {
                        if (process.exitCode() == 0) {
                            QFile pngFile(pngPath);
                            if (pngFile.open(QIODevice::ReadOnly)) {
                                displayData = pngFile.readAll();
                                pngFile.close();
                                loaded = pixmap.loadFromData(displayData);
                            }
                        }
                    }
                }
            }
        }
        if (loaded) {
            thumbnailLabel->setPixmap(pixmap.scaled(50, 50, Qt::KeepAspectRatio));
            // Set tooltip with larger image
            QString base64 = displayData.toBase64();
            thumbnailLabel->setToolTip(QString("<img src='data:image/png;base64,%1' width='200' height='200'>").arg(QString(base64)));
        } else {
            thumbnailLabel->setText("No Image");
        }
        table->setCellWidget(row, 1, thumbnailLabel);
        // Store id in the thumbnail label's property
        thumbnailLabel->setProperty("row_id", id);
        // Other columns
        for (int col = 2; col <= 12; ++col) {
            QTableWidgetItem *item = new QTableWidgetItem(query.value(col).toString());
            table->setItem(row, col, item);
        }
        // View Image button
        QPushButton *viewBtn = new QPushButton("View");
        viewBtn->setProperty("row_id", id);
        connect(viewBtn, &QPushButton::clicked, [this, viewBtn]() {
            viewImage(viewBtn->property("row_id").toInt());
        });
        table->setCellWidget(row, 13, viewBtn);
        row++;
    }
    statusBar()->showMessage("Data loaded");
}

void MainWindow::onCellChanged(int row, int column) {
    if (column == 0 || column == 1 || column == 13) return; // Delete, Thumbnail, View Image
    statusBar()->showMessage("Updating database...");
    QLabel *label = static_cast<QLabel*>(table->cellWidget(row, 1)); // Thumbnail is now column 1
    int id = label->property("row_id").toInt();
    QString columnName;
    switch (column) {
        case 2: columnName = "action"; break;
        case 3: columnName = "category"; break;
        case 4: columnName = "title"; break;
        case 5: columnName = "issue"; break;
        case 6: columnName = "publisher"; break;
        case 7: columnName = "year"; break;
        case 8: columnName = "genre"; break;
        case 9: columnName = "main_characters"; break;
        case 10: columnName = "condition"; break;
        case 11: columnName = "value"; break;
        case 12: columnName = "notes"; break;
    }
    QString value = table->item(row, column)->text();
    QSqlQuery query;
    query.prepare(QString("UPDATE comics SET %1 = ? WHERE id = ?").arg(columnName));
    query.addBindValue(value);
    query.addBindValue(id);
    if (!query.exec()) {
        qDebug() << "DB update failed:" << query.lastError();
        statusBar()->showMessage("Database update failed");
    } else {
        statusBar()->showMessage("Database updated");
    }
}

void MainWindow::onPause() {
    paused = !paused;
    pauseButton->setText(paused ? "Resume" : "Pause");
    if (!paused) {
        processNext();
    }
}

void MainWindow::viewImage(int id) {
    QSqlQuery query;
    query.prepare("SELECT image_data FROM comics WHERE id = ?");
    query.addBindValue(id);
    if (query.exec() && query.next()) {
        QByteArray imageData = query.value(0).toByteArray();
        ImageViewerWidget *viewer = new ImageViewerWidget(imageData, id);
        tabWidget->addTab(viewer, "Image " + QString::number(id));
        tabWidget->setCurrentWidget(viewer);
    }
}

void MainWindow::closeTab(int index) {
    if (index > 0) { // Don't close the main tab
        tabWidget->removeTab(index);
    }
}