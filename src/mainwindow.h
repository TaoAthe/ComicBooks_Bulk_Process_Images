#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QTextEdit>
#include <QCheckBox>
#include <QTableWidget>
#include <QTabWidget>
#include <QSqlDatabase>
#include <QTreeView>
#include <QFileSystemModel>
#include <QListWidget>
#include <QLineEdit>

class ComicProcessor;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onProcessImages();
    void onServerChanged(int index);
    void onModelChanged(int index);
    void onCellChanged(int row, int column);
    void closeTab(int index);
    void viewImage(int id);
    void onPause();
    void onFileDoubleClicked(const QModelIndex &index);
    void onRemoveFromQueue();
    void onClearQueue();
    void onBrowseFolder();
    void onProcessQueue();

private:
    void loadData();
    void processNext();
    void addToQueue(const QString &filePath);
    QPushButton *processButton;
    QPushButton *pauseButton;
    QComboBox *serverCombo;
    QComboBox *modelCombo;
    QLabel *modelInfoLabel;
    QLabel *modelStatusLabel;
    QTextEdit *customPromptEdit;
    QCheckBox *ebayCheckBox;
    QTableWidget *table;
    QTabWidget *tabWidget;
    ComicProcessor *processor;
    QSqlDatabase db;
    QStringList pendingImages;
    bool paused;
    
    // Folder browser and queue components
    QTreeView *folderTreeView;
    QFileSystemModel *fileSystemModel;
    QListWidget *queueListWidget;
    QLineEdit *folderPathEdit;
    QPushButton *browseFolderButton;
    QPushButton *removeFromQueueButton;
    QPushButton *clearQueueButton;
    QPushButton *processQueueButton;
    QLabel *queueCountLabel;
};

#endif // MAINWINDOW_H