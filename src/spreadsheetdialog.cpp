#include "spreadsheetdialog.h"
#include <QHeaderView>
#include <QPixmap>
#include <QLabel>
#include <QHBoxLayout>
#include <QSqlError>
#include <QDebug>
#include <QCoreApplication>

SpreadsheetDialog::SpreadsheetDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle("Comic Inventory Spreadsheet");
    setMinimumSize(1200, 600);

    QVBoxLayout *layout = new QVBoxLayout(this);

    table = new QTableWidget(this);
    table->setColumnCount(20); // Adjust based on fields
    table->setHorizontalHeaderLabels({
        "Thumbnail", "Image Path", "Metadata", "Title", "Issue Number", "Series", "Publisher", "Year", "Genre", "Topic",
        "Condition ID", "Description", "SKU", "eBay Title", "eBay Description", "Price", "Quantity", "Condition", "Category ID", "Picture URL"
    });

    // Make columns editable except thumbnail and image path
    for (int col = 0; col < table->columnCount(); ++col) {
        if (col != 0 && col != 1) { // Thumbnail and Image Path read-only
            // Editable
        } else {
            // Read-only
        }
    }

    connect(table, &QTableWidget::cellChanged, this, &SpreadsheetDialog::onCellChanged);

    refreshButton = new QPushButton("Refresh", this);
    connect(refreshButton, &QPushButton::clicked, this, &SpreadsheetDialog::loadData);

    layout->addWidget(table);
    layout->addWidget(refreshButton);

    // Open DB
    db = QSqlDatabase::addDatabase("QSQLITE", "spreadsheet");
    db.setDatabaseName(QCoreApplication::applicationDirPath() + "/comicvision.db");
    if (!db.open()) {
        qDebug() << "Failed to open DB in spreadsheet";
    }

    loadData();
}

void SpreadsheetDialog::loadData() {
    if (!db.isOpen()) {
        qDebug() << "DB not open in loadData";
        return;
    }

    QSqlQuery query(db);
    if (!query.exec("SELECT * FROM processed_images")) {
        qDebug() << "Query failed:" << query.lastError();
        return;
    }

    table->setRowCount(0); // Clear

    int row = 0;
    while (query.next()) {
        table->insertRow(row);

        // Thumbnail
        QString imagePath = query.value("image_path").toString();
        QString fullImagePath = QCoreApplication::applicationDirPath() + "/images/" + imagePath;
        QPixmap pixmap(fullImagePath);
        if (!pixmap.isNull()) {
            pixmap = pixmap.scaled(64, 64, Qt::KeepAspectRatio);
        } else {
            pixmap = QPixmap(64, 64);
            pixmap.fill(Qt::gray);
        }
        QLabel *label = new QLabel();
        label->setPixmap(pixmap);
        table->setCellWidget(row, 0, label);

        // Other columns
        table->setItem(row, 1, new QTableWidgetItem(imagePath));
        table->item(row, 1)->setFlags(table->item(row, 1)->flags() & ~Qt::ItemIsEditable); // Make image path read-only
        table->setItem(row, 2, new QTableWidgetItem(query.value("metadata").toString()));
        table->item(row, 2)->setFlags(table->item(row, 2)->flags() & ~Qt::ItemIsEditable); // Make metadata read-only
        table->setItem(row, 3, new QTableWidgetItem(query.value("title").toString()));
        table->setItem(row, 4, new QTableWidgetItem(query.value("issue_number").toString()));
        table->setItem(row, 5, new QTableWidgetItem(query.value("series").toString()));
        table->setItem(row, 6, new QTableWidgetItem(query.value("publisher").toString()));
        table->setItem(row, 7, new QTableWidgetItem(query.value("year").toString()));
        table->setItem(row, 8, new QTableWidgetItem(query.value("genre").toString()));
        table->setItem(row, 9, new QTableWidgetItem(query.value("topic").toString()));
        table->setItem(row, 10, new QTableWidgetItem(query.value("condition_id").toString()));
        table->setItem(row, 11, new QTableWidgetItem(query.value("description").toString()));
        table->setItem(row, 12, new QTableWidgetItem(query.value("sku").toString()));
        table->setItem(row, 13, new QTableWidgetItem(query.value("ebay_title").toString()));
        table->setItem(row, 14, new QTableWidgetItem(query.value("ebay_description").toString()));
        table->setItem(row, 15, new QTableWidgetItem(query.value("price").toString()));
        table->setItem(row, 16, new QTableWidgetItem(query.value("quantity").toString()));
        table->setItem(row, 17, new QTableWidgetItem(query.value("condition").toString()));
        table->setItem(row, 18, new QTableWidgetItem(query.value("category_id").toString()));
        table->setItem(row, 19, new QTableWidgetItem(query.value("picture_url").toString()));

        row++;
    }

    table->resizeColumnsToContents();
    qDebug() << "Loaded" << row << "rows";
}

void SpreadsheetDialog::onCellChanged(int row, int column) {
    if (!db.isOpen()) {
        qDebug() << "DB not open in onCellChanged";
        return;
    }

    QString columnName;
    switch (column) {
        case 3: columnName = "title"; break;
        case 4: columnName = "issue_number"; break;
        case 5: columnName = "series"; break;
        case 6: columnName = "publisher"; break;
        case 7: columnName = "year"; break;
        case 8: columnName = "genre"; break;
        case 9: columnName = "topic"; break;
        case 10: columnName = "condition_id"; break;
        case 11: columnName = "description"; break;
        case 12: columnName = "sku"; break;
        case 13: columnName = "ebay_title"; break;
        case 14: columnName = "ebay_description"; break;
        case 15: columnName = "price"; break;
        case 16: columnName = "quantity"; break;
        case 17: columnName = "condition"; break;
        case 18: columnName = "category_id"; break;
        case 19: columnName = "picture_url"; break;
        default: return;
    }

    QString value = table->item(row, column)->text();
    QString imagePath = table->item(row, 1)->text();

    QSqlQuery query(db);
    QString sql = QString("UPDATE processed_images SET %1 = ? WHERE image_path = ?").arg(columnName);
    query.prepare(sql);
    query.addBindValue(value);
    query.addBindValue(imagePath);
    if (!query.exec()) {
        qDebug() << "Update failed:" << query.lastError() << "SQL:" << sql;
    } else {
        qDebug() << "Updated" << columnName << "to" << value << "for" << imagePath;
    }
}