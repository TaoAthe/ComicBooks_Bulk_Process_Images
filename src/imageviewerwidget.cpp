#include "imageviewerwidget.h"
#include <QPixmap>
#include <QWheelEvent>
#include <QProcess>
#include <QTemporaryFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

ImageViewerWidget::ImageViewerWidget(const QByteArray &imageData, int id, QWidget *parent) : QWidget(parent), scaleFactor(1.0), imageData(imageData), id(id) {
    splitter = new QSplitter(Qt::Horizontal, this);
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(splitter);

    // Viewer widget
    QWidget *viewerWidget = new QWidget();
    QVBoxLayout *viewerLayout = new QVBoxLayout(viewerWidget);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *zoomInBtn = new QPushButton("Zoom In");
    QPushButton *zoomOutBtn = new QPushButton("Zoom Out");
    QPushButton *fitBtn = new QPushButton("Fit to Window");
    QPushButton *actualBtn = new QPushButton("Actual Size");
    QPushButton *gradeBtn = new QPushButton("Grade Comic");
    buttonLayout->addWidget(zoomInBtn);
    buttonLayout->addWidget(zoomOutBtn);
    buttonLayout->addWidget(fitBtn);
    buttonLayout->addWidget(actualBtn);
    buttonLayout->addWidget(gradeBtn);
    buttonLayout->addStretch();
    viewerLayout->addLayout(buttonLayout);

    view = new ZoomableGraphicsView(viewerWidget);
    scene = new QGraphicsScene(viewerWidget);
    view->setScene(scene);
    view->setDragMode(QGraphicsView::ScrollHandDrag);
    view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);

    QPixmap pixmap;
    bool loaded = pixmap.loadFromData(imageData);
    if (!loaded && imageData.size() > 12 && imageData.startsWith("RIFF") && imageData.mid(8, 4) == "WEBP") {
        // Convert WebP to PNG for display
        QTemporaryFile tempWebp;
        if (tempWebp.open()) {
            tempWebp.write(imageData);
            tempWebp.close();
            QString webpPath = tempWebp.fileName();
            QTemporaryFile tempPng;
            tempPng.setFileTemplate(QDir::tempPath() + "/viewer_XXXXXX.png");
            if (tempPng.open()) {
                QString pngPath = tempPng.fileName();
                tempPng.close();
                QProcess process;
                process.start("magick", QStringList() << "convert" << webpPath << pngPath);
                if (process.waitForFinished(10000)) {
                    if (process.exitCode() == 0) {
                        QFile pngFile(pngPath);
                        if (pngFile.open(QIODevice::ReadOnly)) {
                            QByteArray pngData = pngFile.readAll();
                            pngFile.close();
                            loaded = pixmap.loadFromData(pngData);
                        }
                    }
                }
            }
        }
    }
    if (!loaded) {
        QLabel *errorLabel = new QLabel("Error: Image data is invalid or corrupted");
        viewerLayout->addWidget(errorLabel);
        return;
    }

    pixmapItem = new QGraphicsPixmapItem(pixmap);
    scene->addItem(pixmapItem);
    viewerLayout->addWidget(view);

    connect(zoomInBtn, &QPushButton::clicked, this, &ImageViewerWidget::zoomIn);
    connect(zoomOutBtn, &QPushButton::clicked, this, &ImageViewerWidget::zoomOut);
    connect(fitBtn, &QPushButton::clicked, this, &ImageViewerWidget::fitToWindow);
    connect(actualBtn, &QPushButton::clicked, this, &ImageViewerWidget::actualSize);
    connect(gradeBtn, &QPushButton::clicked, this, &ImageViewerWidget::onGradeComic);

    splitter->addWidget(viewerWidget);

    // Results panel
    QWidget *resultsWidget = new QWidget();
    QVBoxLayout *resultsLayout = new QVBoxLayout(resultsWidget);
    
    QLabel *promptLabel = new QLabel("Additional Instructions (optional):");
    resultsLayout->addWidget(promptLabel);
    
    additionalPromptEdit = new QTextEdit();
    additionalPromptEdit->setPlaceholderText("Enter any additional grading instructions here...");
    additionalPromptEdit->setMaximumHeight(100);  // Limit height
    resultsLayout->addWidget(additionalPromptEdit);
    
    progressBar = new QProgressBar();
    progressBar->setRange(0, 7);  // 7 phases total
    progressBar->setValue(0);
    progressBar->setFormat("Starting grading...");
    resultsLayout->addWidget(progressBar);
    
    resultsEdit = new QTextEdit();
    resultsEdit->setReadOnly(true);
    resultsEdit->setPlainText("Grading results will appear here...");
    resultsLayout->addWidget(resultsEdit);
    
    splitter->addWidget(resultsWidget);
    resultsWidget->hide();  // Initially hidden

    splitter->setStretchFactor(0, 3);  // Viewer takes more space
    splitter->setStretchFactor(1, 1);

    gradingProcess = nullptr;
    tempFile = nullptr;
}

void ImageViewerWidget::zoomIn() {
    scaleFactor *= 1.25;
    view->scale(1.25, 1.25);
}

void ImageViewerWidget::zoomOut() {
    scaleFactor /= 1.25;
    view->scale(1.0 / 1.25, 1.0 / 1.25);
}

void ImageViewerWidget::fitToWindow() {
    view->fitInView(scene->sceneRect(), Qt::KeepAspectRatio);
    // Update scaleFactor if needed, but for simplicity, leave
}

void ImageViewerWidget::actualSize() {
    view->resetTransform();
    scaleFactor = 1.0;
}

void ImageViewerWidget::onGradeComic() {
    if (gradingProcess) {
        // Already grading
        return;
    }

    resultsEdit->parentWidget()->show();  // Show the results widget
    resultsEdit->clear();
    resultsEdit->append("Starting comic grading...\n");
    
    progressBar->setValue(0);
    progressBar->setFormat("Starting grading...");

    // Save image to temp file
    if (tempFile) {
        delete tempFile;
    }
    tempFile = new QTemporaryFile(QDir::tempPath() + "/comic_grade_XXXXXX.png");
    if (!tempFile->open()) {
        resultsEdit->append("Error: Failed to create temp file.\n");
        return;
    }
    tempFile->write(imageData);
    tempFile->close();
    QString imagePath = tempFile->fileName();

    gradingProcess = new QProcess(this);
    connect(gradingProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &ImageViewerWidget::onGradingFinished);
    connect(gradingProcess, &QProcess::readyReadStandardOutput, this, &ImageViewerWidget::onGradingOutput);

    // Set working directory to the script location
    QString scriptDir = QCoreApplication::applicationDirPath() + "/../../";
    gradingProcess->setWorkingDirectory(scriptDir);

    QString additionalPrompt = additionalPromptEdit->toPlainText().trimmed();

    gradingProcess->start("python", QStringList() << "phased_grading_engine.py" << imagePath << additionalPrompt);
}

void ImageViewerWidget::onGradingOutput() {
    if (!gradingProcess) return;
    QByteArray output = gradingProcess->readAllStandardOutput();
    QString text = QString::fromUtf8(output);
    QStringList lines = text.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8());
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            QString phase = obj["phase"].toString();
            QString status = obj["status"].toString();
            if (status == "starting") {
                resultsEdit->append(QString("## Starting %1 analysis...\n").arg(phase));
                if (phase == "spine") {
                    progressBar->setValue(0);
                    progressBar->setFormat("Phase 1/7: Spine Analysis");
                } else if (phase == "corners") {
                    progressBar->setValue(1);
                    progressBar->setFormat("Phase 2/7: Corner Analysis");
                } else if (phase == "surface") {
                    progressBar->setValue(2);
                    progressBar->setFormat("Phase 3/7: Surface Analysis");
                } else if (phase == "structural") {
                    progressBar->setValue(3);
                    progressBar->setFormat("Phase 4/7: Structural Analysis");
                } else if (phase == "color_gloss") {
                    progressBar->setValue(4);
                    progressBar->setFormat("Phase 5/7: Color/Gloss Analysis");
                } else if (phase == "paper") {
                    progressBar->setValue(5);
                    progressBar->setFormat("Phase 6/7: Paper Quality Analysis");
                } else if (phase == "synthesis") {
                    progressBar->setValue(6);
                    progressBar->setFormat("Phase 7/7: Final Synthesis");
                }
            } else if (status == "completed") {
                resultsEdit->append(QString("## %1 analysis completed\n").arg(phase));
                QJsonObject data = obj["data"].toObject();
                for (auto it = data.begin(); it != data.end(); ++it) {
                    QString key = it.key();
                    QJsonValue value = it.value();
                    if (value.isDouble()) {
                        resultsEdit->append(QString("- %1: %2\n").arg(key, QString::number(value.toDouble())));
                    } else if (value.isString()) {
                        resultsEdit->append(QString("- %1: %2\n").arg(key, value.toString()));
                    } else if (value.isArray()) {
                        QJsonArray arr = value.toArray();
                        resultsEdit->append(QString("- %1: %2 items\n").arg(key, QString::number(arr.size())));
                    } else {
                        resultsEdit->append(QString("- %1: %2\n").arg(key, value.toString()));
                    }
                }
                resultsEdit->append("\n");
            }
        } else {
            // Non-JSON line, append directly to results
            resultsEdit->append(line + "\n");
        }
    }
}

void ImageViewerWidget::onGradingFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    progressBar->setValue(7);
    progressBar->setFormat("Grading Complete");
    
    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        resultsEdit->append("\nGrading completed successfully.\n");
    } else {
        resultsEdit->append("\nGrading failed.\n");
        QByteArray error = gradingProcess->readAllStandardError();
        resultsEdit->append(QString::fromUtf8(error));
    }
    gradingProcess->deleteLater();
    gradingProcess = nullptr;
    if (tempFile) {
        delete tempFile;
        tempFile = nullptr;
    }
}