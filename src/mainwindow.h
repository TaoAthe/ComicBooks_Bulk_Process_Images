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

private:
    void loadData();
    void processNext();
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
};

#endif // MAINWINDOW_H