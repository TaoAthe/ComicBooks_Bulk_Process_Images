#ifndef SPREADSHEETDIALOG_H
#define SPREADSHEETDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QSqlDatabase>
#include <QSqlQuery>

class SpreadsheetDialog : public QDialog {
    Q_OBJECT

public:
    SpreadsheetDialog(QWidget *parent = nullptr);

private slots:
    void loadData();
    void onCellChanged(int row, int column);

private:
    QTableWidget *table;
    QPushButton *refreshButton;
    QSqlDatabase db;
};

#endif // SPREADSHEETDIALOG_H