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
    // Query the actual comics table (not the old processed_images table)
    if (!query.exec("SELECT id, image_path, notes, title, issue, main_characters, publisher, year, genre, action, condition, value, category FROM comics ORDER BY id")) {
        qDebug() << "Query failed:" << query.lastError();
        return;
    }

    // Block cellChanged signals during load to avoid spurious UPDATE calls
    table->blockSignals(true);
    table->setRowCount(0);

    int row = 0;
    while (query.next()) {
        table->insertRow(row);

        int rowId = query.value("id").toInt();

        // Thumbnail (col 0) — image_path is already the full absolute path
        QString imagePath = query.value("image_path").toString();
        QPixmap pixmap(imagePath);
        if (!pixmap.isNull()) {
            pixmap = pixmap.scaled(64, 64, Qt::KeepAspectRatio);
        } else {
            pixmap = QPixmap(64, 64);
            pixmap.fill(Qt::gray);
        }
        QLabel *label = new QLabel();
        label->setPixmap(pixmap);
        table->setCellWidget(row, 0, label);

        // Image Path (col 1) — read-only; store the DB row id in UserRole for onCellChanged
        auto *pathItem = new QTableWidgetItem(imagePath);
        pathItem->setFlags(pathItem->flags() & ~Qt::ItemIsEditable);
        pathItem->setData(Qt::UserRole, rowId);
        table->setItem(row, 1, pathItem);

        // Notes / Metadata (col 2) — read-only
        auto *notesItem = new QTableWidgetItem(query.value("notes").toString());
        notesItem->setFlags(notesItem->flags() & ~Qt::ItemIsEditable);
        table->setItem(row, 2, notesItem);

        // Editable fields mapped to actual comics columns
        table->setItem(row, 3,  new QTableWidgetItem(query.value("title").toString()));
        table->setItem(row, 4,  new QTableWidgetItem(query.value("issue").toString()));
        table->setItem(row, 5,  new QTableWidgetItem(query.value("main_characters").toString()));
        table->setItem(row, 6,  new QTableWidgetItem(query.value("publisher").toString()));
        table->setItem(row, 7,  new QTableWidgetItem(query.value("year").toString()));
        table->setItem(row, 8,  new QTableWidgetItem(query.value("genre").toString()));
        table->setItem(row, 9,  new QTableWidgetItem(query.value("action").toString()));
        table->setItem(row, 10, new QTableWidgetItem(query.value("condition").toString()));
        table->setItem(row, 11, new QTableWidgetItem(query.value("notes").toString()));
        table->setItem(row, 12, new QTableWidgetItem(""));  // SKU — not yet in schema
        table->setItem(row, 13, new QTableWidgetItem(""));  // eBay Title — not yet in schema
        table->setItem(row, 14, new QTableWidgetItem(""));  // eBay Description — not yet in schema
        table->setItem(row, 15, new QTableWidgetItem(query.value("value").toString()));
        table->setItem(row, 16, new QTableWidgetItem(""));  // Quantity — not yet in schema
        table->setItem(row, 17, new QTableWidgetItem(query.value("condition").toString()));
        table->setItem(row, 18, new QTableWidgetItem(query.value("category").toString()));
        table->setItem(row, 19, new QTableWidgetItem(imagePath));  // Picture URL

        row++;
    }

    table->blockSignals(false);
    table->resizeColumnsToContents();
    qDebug() << "Loaded" << row << "rows from comics table";
}

void SpreadsheetDialog::onCellChanged(int row, int column) {
    if (!db.isOpen()) {
        qDebug() << "DB not open in onCellChanged";
        return;
    }

    // Map column index to the actual comics table column name
    QString columnName;
    switch (column) {
        case 3:  columnName = "title"; break;
        case 4:  columnName = "issue"; break;
        case 5:  columnName = "main_characters"; break;
        case 6:  columnName = "publisher"; break;
        case 7:  columnName = "year"; break;
        case 8:  columnName = "genre"; break;
        case 9:  columnName = "action"; break;
        case 10: columnName = "condition"; break;
        case 11: columnName = "notes"; break;
        case 15: columnName = "value"; break;
        case 17: columnName = "condition"; break;
        case 18: columnName = "category"; break;
        default: return;  // Read-only or not yet in schema
    }

    // Retrieve the DB row id stored in the Image Path item
    QTableWidgetItem *pathItem = table->item(row, 1);
    if (!pathItem) return;
    int rowId = pathItem->data(Qt::UserRole).toInt();
    if (rowId <= 0) return;

    QString value = table->item(row, column) ? table->item(row, column)->text() : QString();

    QSqlQuery query(db);
    QString sql = QStringLiteral("UPDATE comics SET %1 = ? WHERE id = ?").arg(columnName);
    query.prepare(sql);
    query.addBindValue(value);
    query.addBindValue(rowId);
    if (!query.exec()) {
        qDebug() << "Update failed:" << query.lastError() << "SQL:" << sql;
    } else {
        qDebug() << "Updated" << columnName << "=" << value << "for id" << rowId;
    }
}