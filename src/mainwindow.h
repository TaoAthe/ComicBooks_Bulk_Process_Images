#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QComboBox>
#include <QTextEdit>

class ComicProcessor;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onProcessImages();
    void onServerChanged(int index);
    void onModelChanged(int index);

private:
    QPushButton *processButton;
    QLabel *statusLabel;
    QComboBox *serverCombo;
    QComboBox *modelCombo;
    QLabel *modelInfoLabel;
    QLabel *modelStatusLabel;
    QTextEdit *customPromptEdit;
    ComicProcessor *processor;
};

#endif // MAINWINDOW_H