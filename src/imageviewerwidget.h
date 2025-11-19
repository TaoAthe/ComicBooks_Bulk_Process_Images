#ifndef IMAGEVIEWERWIDGET_H
#define IMAGEVIEWERWIDGET_H

#include <QWidget>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QWheelEvent>
#include <QSplitter>
#include <QProgressBar>
#include <QTextEdit>
#include <QProcess>
#include <QTemporaryFile>

class ZoomableGraphicsView : public QGraphicsView {
public:
    explicit ZoomableGraphicsView(QWidget *parent = nullptr) : QGraphicsView(parent) {}
protected:
    void wheelEvent(QWheelEvent *event) override {
        if (event->angleDelta().y() > 0) {
            scale(1.25, 1.25);
        } else {
            scale(1.0 / 1.25, 1.0 / 1.25);
        }
    }
};

class ImageViewerWidget : public QWidget {
    Q_OBJECT

public:
    explicit ImageViewerWidget(const QByteArray &imageData, int id, QWidget *parent = nullptr);
    const QByteArray &getImageData() const { return imageData; }

signals:
    void gradeComicRequested();

private slots:
    void zoomIn();
    void zoomOut();
    void fitToWindow();
    void actualSize();
    void onGradeComic();
    void onGradingOutput();
    void onGradingFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    ZoomableGraphicsView *view;
    QGraphicsScene *scene;
    QGraphicsPixmapItem *pixmapItem;
    double scaleFactor;
    QByteArray imageData;
    int id;
    QSplitter *splitter;
    QTextEdit *additionalPromptEdit;
    QTextEdit *resultsEdit;
    QProgressBar *progressBar;
    QProcess *gradingProcess;
    QTemporaryFile *tempFile;
};

#endif // IMAGEVIEWERWIDGET_H