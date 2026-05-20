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
#include <QHBoxLayout>
#include <QComboBox>
#include <QTextEdit>
#include <QProcess>
#include <QTemporaryFile>
#include <QPushButton>
#include <QCloseEvent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSplitter>
#include <QGroupBox>
#include <QFileDialog>
#include <QTreeView>
#include <QFileSystemModel>
#include <QListWidget>
#include <QLineEdit>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    // Set up GUI
    tabWidget = new QTabWidget(this);
    tabWidget->setTabsClosable(true);
    connect(tabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTab);
    QWidget *mainTab = new QWidget();
    
    // Main splitter - horizontal layout with left panel and right content
    QSplitter *mainSplitter = new QSplitter(Qt::Horizontal, mainTab);
    
    // LEFT PANEL - Folder Browser and Queue
    QWidget *leftPanel = new QWidget();
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(5, 5, 5, 5);
    
    // Folder browser section
    QGroupBox *folderGroupBox = new QGroupBox("Folder Browser", leftPanel);
    QVBoxLayout *folderLayout = new QVBoxLayout(folderGroupBox);
    
    // Folder path input
    QHBoxLayout *pathLayout = new QHBoxLayout();
    folderPathEdit = new QLineEdit(folderGroupBox);
    folderPathEdit->setPlaceholderText("Select folder to browse...");
    folderPathEdit->setReadOnly(true);
    browseFolderButton = new QPushButton("Browse...", folderGroupBox);
    pathLayout->addWidget(folderPathEdit);
    pathLayout->addWidget(browseFolderButton);
    folderLayout->addLayout(pathLayout);
    
    // Tree view for folder browsing
    folderTreeView = new QTreeView(folderGroupBox);
    fileSystemModel = new QFileSystemModel(this);
    
    // Set up file filters for images
    QStringList filters;
    filters << "*.jpg" << "*.jpeg" << "*.png" << "*.gif" << "*.bmp" << "*.webp" << "*.tiff" << "*.tif";
    fileSystemModel->setNameFilters(filters);
    fileSystemModel->setNameFilterDisables(false);
    
    QString defaultPath = QCoreApplication::applicationDirPath() + "/images";
    fileSystemModel->setRootPath(defaultPath);
    folderTreeView->setModel(fileSystemModel);
    folderTreeView->setRootIndex(fileSystemModel->index(defaultPath));
    folderTreeView->setColumnWidth(0, 200);
    folderTreeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    folderTreeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    folderPathEdit->setText(defaultPath);
    
    folderLayout->addWidget(folderTreeView);
    
    QLabel *instructionLabel = new QLabel("Double-click files to add to queue", folderGroupBox);
    instructionLabel->setStyleSheet("color: gray; font-style: italic;");
    folderLayout->addWidget(instructionLabel);
    
    leftLayout->addWidget(folderGroupBox);
    
    // Processing queue section
    QGroupBox *queueGroupBox = new QGroupBox("Processing Queue", leftPanel);
    QVBoxLayout *queueLayout = new QVBoxLayout(queueGroupBox);
    
    queueCountLabel = new QLabel("Items in queue: 0", queueGroupBox);
    queueCountLabel->setStyleSheet("font-weight: bold;");
    queueLayout->addWidget(queueCountLabel);
    
    queueListWidget = new QListWidget(queueGroupBox);
    queueListWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    queueLayout->addWidget(queueListWidget);
    
    // Queue control buttons
    QHBoxLayout *queueButtonLayout = new QHBoxLayout();
    removeFromQueueButton = new QPushButton("Remove Selected", queueGroupBox);
    clearQueueButton = new QPushButton("Clear All", queueGroupBox);
    queueButtonLayout->addWidget(removeFromQueueButton);
    queueButtonLayout->addWidget(clearQueueButton);
    queueLayout->addLayout(queueButtonLayout);
    
    processQueueButton = new QPushButton("Process Queue", queueGroupBox);
    processQueueButton->setStyleSheet("QPushButton { font-weight: bold; padding: 8px; }");
    processQueueButton->setEnabled(false);
    queueLayout->addWidget(processQueueButton);
    
    leftLayout->addWidget(queueGroupBox);
    
    // Set size policies
    leftPanel->setMinimumWidth(300);
    leftPanel->setMaximumWidth(500);
    
    // RIGHT PANEL - Main content
    QWidget *rightPanel = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(rightPanel);

    serverCombo = new QComboBox(rightPanel);
    serverCombo->addItem("Ollama", ComicProcessor::Ollama);
    serverCombo->addItem("LM Studio", ComicProcessor::LMStudio);
    serverCombo->setCurrentIndex(1); // Default to LM Studio
    modelCombo = new QComboBox(rightPanel);
    modelInfoLabel = new QLabel("Model Info: Select a model", rightPanel);
    modelInfoLabel->setWordWrap(true);
    modelStatusLabel = new QLabel("Model Status: Not Ready", rightPanel);
    modelStatusLabel->setWordWrap(true);

    ebayCheckBox = new QCheckBox("Generate eBay Listing", rightPanel);

    table = new QTableWidget(rightPanel);
    table->setColumnCount(14); // Delete + Thumbnail + 11 columns + View Image
    QStringList headers = {"", "Thumbnail", "Action", "Category", "Title", "Issue", "Publisher", "Year", "Genre", "Main Characters", "Condition", "Value", "Notes", "View Image"};
    table->setHorizontalHeaderLabels(headers);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    processButton = new QPushButton("Process Images Folder", rightPanel);
    processButton->setEnabled(false); // Initially disabled

    pauseButton = new QPushButton("Pause", rightPanel);
    pauseButton->setEnabled(false); // Initially disabled

    layout->addWidget(new QLabel("Server:", rightPanel));
    layout->addWidget(serverCombo);
    layout->addWidget(new QLabel("Model:", rightPanel));
    layout->addWidget(modelCombo);
    layout->addWidget(modelInfoLabel);
    layout->addWidget(modelStatusLabel);
    layout->addWidget(ebayCheckBox);
    layout->addWidget(table);
    
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addWidget(processButton);
    buttonLayout->addWidget(pauseButton);
    layout->addLayout(buttonLayout);

    // Add panels to splitter
    mainSplitter->addWidget(leftPanel);
    mainSplitter->addWidget(rightPanel);
    mainSplitter->setStretchFactor(0, 1);  // Left panel
    mainSplitter->setStretchFactor(1, 3);  // Right panel gets more space
    
    // Set up main tab layout
    QVBoxLayout *mainTabLayout = new QVBoxLayout(mainTab);
    mainTabLayout->setContentsMargins(0, 0, 0, 0);
    mainTabLayout->addWidget(mainSplitter);

    // Connect signals
    connect(processButton, &QPushButton::clicked, this, &MainWindow::onProcessImages);
    connect(pauseButton, &QPushButton::clicked, this, &MainWindow::onPause);
    connect(serverCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onServerChanged);
    connect(modelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onModelChanged);
    connect(table, &QTableWidget::cellChanged, this, &MainWindow::onCellChanged);
    connect(folderTreeView, &QTreeView::doubleClicked, this, &MainWindow::onFileDoubleClicked);
    connect(browseFolderButton, &QPushButton::clicked, this, &MainWindow::onBrowseFolder);
    connect(removeFromQueueButton, &QPushButton::clicked, this, &MainWindow::onRemoveFromQueue);
    connect(clearQueueButton, &QPushButton::clicked, this, &MainWindow::onClearQueue);
    connect(processQueueButton, &QPushButton::clicked, this, &MainWindow::onProcessQueue);

    tabWidget->addTab(mainTab, "Main");
    setCentralWidget(tabWidget);
    setWindowTitle("ComicVision Sorter - Stage 1");
    resize(1400, 700);

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
    onServerChanged(serverCombo->currentIndex());
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
    } else {
        QMessageBox::critical(this, "Error", "Failed to load image from database: " + query.lastError().text());
        statusBar()->showMessage("Error loading image");
    }
}

void MainWindow::onFileDoubleClicked(const QModelIndex &index) {
    if (fileSystemModel->isDir(index)) {
        // Navigate into the directory: re-root the tree view
        QString dirPath = fileSystemModel->filePath(index);
        fileSystemModel->setRootPath(dirPath);
        folderTreeView->setRootIndex(fileSystemModel->index(dirPath));
        folderPathEdit->setText(dirPath);
        statusBar()->showMessage("Browsing: " + dirPath);
        return;
    }
    QString filePath = fileSystemModel->filePath(index);
    QFileInfo fileInfo(filePath);
    
    // Validate file exists and is readable
    if (!fileInfo.exists()) {
        QMessageBox::warning(this, "File Not Found", 
            "The file does not exist:\n" + filePath);
        statusBar()->showMessage("Error: File not found");
        return;
    }
    
    if (!fileInfo.isReadable()) {
        QMessageBox::warning(this, "File Not Readable", 
            "Cannot read the file (check permissions):\n" + filePath);
        statusBar()->showMessage("Error: File not readable");
        return;
    }
    
    // Check if file is a valid image
    QImageReader reader(filePath);
    if (!reader.canRead()) {
        QMessageBox::warning(this, "Invalid Image", 
            "The file is not a valid image format:\n" + filePath + 
            "\n\nSupported formats: JPG, PNG, GIF, BMP, WEBP, TIFF");
        statusBar()->showMessage("Error: Invalid image format");
        return;
    }
    
    addToQueue(filePath);
}

void MainWindow::addToQueue(const QString &filePath) {
    // Check if already in queue
    for (int i = 0; i < queueListWidget->count(); ++i) {
        QListWidgetItem *item = queueListWidget->item(i);
        if (item->data(Qt::UserRole).toString() == filePath) {
            QMessageBox::information(this, "Already in Queue", 
                "This file is already in the processing queue:\n" + QFileInfo(filePath).fileName());
            return;
        }
    }
    
    QFileInfo fileInfo(filePath);
    QListWidgetItem *item = new QListWidgetItem(fileInfo.fileName());
    item->setData(Qt::UserRole, filePath);
    item->setToolTip(filePath);
    queueListWidget->addItem(item);
    
    // Update queue count
    int count = queueListWidget->count();
    queueCountLabel->setText(QString("Items in queue: %1").arg(count));
    
    // Enable process button if model is ready
    if (count > 0 && !modelCombo->currentText().isEmpty()) {
        processQueueButton->setEnabled(true);
    }
    
    statusBar()->showMessage(QString("Added to queue: %1").arg(fileInfo.fileName()));
}

void MainWindow::onRemoveFromQueue() {
    QList<QListWidgetItem*> selectedItems = queueListWidget->selectedItems();
    
    if (selectedItems.isEmpty()) {
        QMessageBox::information(this, "No Selection", 
            "Please select one or more items to remove from the queue.");
        return;
    }
    
    for (QListWidgetItem *item : selectedItems) {
        delete item;
    }
    
    int count = queueListWidget->count();
    queueCountLabel->setText(QString("Items in queue: %1").arg(count));
    
    if (count == 0) {
        processQueueButton->setEnabled(false);
    }
    
    statusBar()->showMessage(QString("Removed %1 item(s) from queue").arg(selectedItems.count()));
}

void MainWindow::onClearQueue() {
    int count = queueListWidget->count();
    
    if (count == 0) {
        QMessageBox::information(this, "Queue Empty", "The processing queue is already empty.");
        return;
    }
    
    QMessageBox::StandardButton reply = QMessageBox::question(this, "Clear Queue",
        QString("Are you sure you want to remove all %1 item(s) from the queue?").arg(count),
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        queueListWidget->clear();
        queueCountLabel->setText("Items in queue: 0");
        processQueueButton->setEnabled(false);
        statusBar()->showMessage("Queue cleared");
    }
}

void MainWindow::onBrowseFolder() {
    QString currentPath = folderPathEdit->text();
    if (currentPath.isEmpty()) {
        currentPath = QCoreApplication::applicationDirPath();
    }
    
    QString folderPath = QFileDialog::getExistingDirectory(this, 
        "Select Folder to Browse", currentPath,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    
    if (!folderPath.isEmpty()) {
        QDir dir(folderPath);
        if (!dir.exists()) {
            QMessageBox::critical(this, "Error", 
                "The selected folder does not exist:\n" + folderPath);
            return;
        }
        
        if (!dir.isReadable()) {
            QMessageBox::critical(this, "Error", 
                "Cannot read the selected folder (check permissions):\n" + folderPath);
            return;
        }
        
        folderPathEdit->setText(folderPath);
        fileSystemModel->setRootPath(folderPath);
        folderTreeView->setRootIndex(fileSystemModel->index(folderPath));
        statusBar()->showMessage("Browsing folder: " + folderPath);
    }
}

void MainWindow::onProcessQueue() {
    int count = queueListWidget->count();
    
    if (count == 0) {
        QMessageBox::warning(this, "Empty Queue", 
            "The processing queue is empty. Double-click files in the folder browser to add them.");
        return;
    }
    
    QString selectedModel = modelCombo->currentText();
    if (selectedModel.isEmpty()) {
        QMessageBox::warning(this, "No Model Selected", 
            "Please select an AI model before processing images.");
        return;
    }
    
    // Check if model is ready
    if (!processButton->isEnabled() && !modelStatusLabel->text().contains("Ready")) {
        QMessageBox::warning(this, "Model Not Ready", 
            "The selected model is not ready for processing. Please wait or select a different model.");
        return;
    }
    
    // Collect all queue items into pendingImages
    pendingImages.clear();
    for (int i = 0; i < count; ++i) {
        QString filePath = queueListWidget->item(i)->data(Qt::UserRole).toString();
        
        // Validate each file still exists
        if (!QFile::exists(filePath)) {
            QMessageBox::warning(this, "File Not Found", 
                QString("File no longer exists and will be skipped:\n%1").arg(filePath));
            continue;
        }
        
        pendingImages.append(filePath);
    }
    
    if (pendingImages.isEmpty()) {
        QMessageBox::critical(this, "Error", 
            "No valid files to process. All files in the queue are missing or invalid.");
        return;
    }
    
    // Clear the queue after collecting files
    queueListWidget->clear();
    queueCountLabel->setText("Items in queue: 0");
    processQueueButton->setEnabled(false);
    
    // Start processing
    statusBar()->showMessage(QString("Processing %1 image(s) from queue...").arg(pendingImages.count()));
    processButton->setEnabled(false);
    pauseButton->setEnabled(true);
    paused = false;
    pauseButton->setText("Pause");
    
    processor->setModel(selectedModel);
    processNext();
}

void MainWindow::closeTab(int index) {
    if (index > 0) { // Don't close the main tab
        tabWidget->removeTab(index);
    }
}