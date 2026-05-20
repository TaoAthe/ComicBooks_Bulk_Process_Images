// test_ebaycsv.cpp
// Qt Test suite for eBay bulk CSV export logic.
//
// These tests verify the data-mapping and CSV formatting rules defined in
// specs2.txt and docs/TODO.md.  They will fail to compile until the
// EbayCsvExporter class (or equivalent function) is implemented.
// Remove the #ifdef EBAY_EXPORTER_IMPLEMENTED guards as you build each piece.
//
// Run:  TestEbayCsv.exe
// Build: included via tests/CMakeLists.txt

#include <QtTest/QtTest>
#include <QFile>
#include <QTextStream>
#include <QTemporaryFile>
#include <QStringList>

#include "comicprocessor.h"

// ─────────────────────────────────────────────────────────────────────────────
// Standalone CSV helpers (will live in EbayCsvExporter once implemented)
// Duplicated here so the tests can run before the class exists.
// ─────────────────────────────────────────────────────────────────────────────
namespace CsvHelper {

// Quote a field: wrap in double-quotes, escape internal " as ""
QString quoteField(const QString &value) {
    QString escaped = value;
    escaped.replace("\"", "\"\"");
    return "\"" + escaped + "\"";
}

// Build a single CSV row from a ComicEntry
QString buildEbayRow(const ComicEntry &entry) {
    QStringList fields;
    fields << quoteField(entry.action.isEmpty() ? "Add" : entry.action);
    fields << quoteField("");                               // ItemID (empty for new)
    fields << quoteField(entry.sku);
    fields << quoteField(entry.ebay_title.isEmpty() ? entry.title : entry.ebay_title);
    fields << quoteField(entry.ebay_description.isEmpty() ? entry.notes : entry.ebay_description);
    fields << quoteField(entry.category_id.isEmpty() ? "180089" : entry.category_id);
    fields << quoteField(entry.price);
    fields << quoteField(entry.quantity.isEmpty() ? "1" : entry.quantity);
    fields << quoteField(entry.condition.isEmpty() ? "3000" : entry.condition);
    fields << quoteField(entry.picture_url);
    return fields.join(",");
}

// Header row
QString ebayHeaderRow() {
    return "\"Action\",\"ItemID\",\"SKU\",\"Title\",\"Description\","
           "\"Category\",\"Price\",\"Quantity\",\"Condition\",\"PictureURL1\"";
}

// Write a list of entries to a QTextStream (UTF-8)
void writeEbayCsv(const QList<ComicEntry> &entries, QTextStream &out) {
    out << ebayHeaderRow() << "\n";
    for (const ComicEntry &e : entries) {
        out << buildEbayRow(e) << "\n";
    }
}

} // namespace CsvHelper

// ─────────────────────────────────────────────────────────────────────────────
class TestEbayCsv : public QObject {
    Q_OBJECT

private slots:

    // ── Header row contains required eBay columns ────────────────────────
    void headerRow_containsRequiredColumns() {
        QString header = CsvHelper::ebayHeaderRow();
        QVERIFY(header.contains("Action"));
        QVERIFY(header.contains("SKU"));
        QVERIFY(header.contains("Title"));
        QVERIFY(header.contains("Description"));
        QVERIFY(header.contains("Category"));
        QVERIFY(header.contains("Price"));
        QVERIFY(header.contains("Quantity"));
        QVERIFY(header.contains("Condition"));
        QVERIFY(header.contains("PictureURL1"));
    }

    // ── quoteField wraps value in double-quotes ──────────────────────────
    void quoteField_addsDoubleQuotes() {
        QCOMPARE(CsvHelper::quoteField("Batman"), QStringLiteral("\"Batman\""));
    }

    // ── quoteField escapes internal double-quotes by doubling ────────────
    void quoteField_escapesInternalQuotes() {
        QCOMPARE(CsvHelper::quoteField("He said \"hello\""),
                 QStringLiteral("\"He said \"\"hello\"\"\""));
    }

    // ── quoteField handles commas without breaking CSV ───────────────────
    void quoteField_handlesCommas() {
        QString result = CsvHelper::quoteField("Title, Subtitle");
        // Must be wrapped in quotes so the comma is not a column delimiter
        QVERIFY(result.startsWith('"'));
        QVERIFY(result.endsWith('"'));
        QVERIFY(result.contains(","));
    }

    // ── quoteField handles empty string ──────────────────────────────────
    void quoteField_emptyString() {
        QCOMPARE(CsvHelper::quoteField(""), QStringLiteral("\"\""));
    }

    // ── Row has exactly 10 columns ───────────────────────────────────────
    void buildRow_hasTenColumns() {
        ComicEntry e;
        e.title       = "Spider-Man";
        e.issueNumber = "#1";
        e.publisher   = "Marvel";
        e.sku         = "SPM-1-ABC123";
        e.ebay_title  = "Spider-Man #1 - Marvel";
        e.price       = "29.99";

        QString row = CsvHelper::buildEbayRow(e);
        // Split on commas that are NOT inside quotes (simple count for well-formed rows)
        // Each field is quoted, so commas inside quotes don't count.
        // Count top-level commas = columns - 1 = 9.
        int topLevelCommas = 0;
        bool inQuotes = false;
        for (QChar ch : row) {
            if (ch == '"') inQuotes = !inQuotes;
            else if (ch == ',' && !inQuotes) ++topLevelCommas;
        }
        QCOMPARE(topLevelCommas, 9);
    }

    // ── Action defaults to "Add" when empty ──────────────────────────────
    void buildRow_actionDefaultsToAdd() {
        ComicEntry e;
        e.action = "";
        QString row = CsvHelper::buildEbayRow(e);
        QVERIFY(row.startsWith("\"Add\""));
    }

    // ── Condition defaults to "3000" when empty ──────────────────────────
    void buildRow_conditionDefault3000() {
        ComicEntry e;
        e.condition = "";
        QString row = CsvHelper::buildEbayRow(e);
        QVERIFY(row.contains("\"3000\""));
    }

    // ── Quantity defaults to "1" when empty ──────────────────────────────
    void buildRow_quantityDefault1() {
        ComicEntry e;
        e.quantity = "";
        QString row = CsvHelper::buildEbayRow(e);
        QVERIFY(row.contains("\"1\""));
    }

    // ── Category defaults to "180089" when empty ─────────────────────────
    void buildRow_categoryDefault180089() {
        ComicEntry e;
        e.category_id = "";
        QString row = CsvHelper::buildEbayRow(e);
        QVERIFY(row.contains("\"180089\""));
    }

    // ── eBay title falls back to title when ebay_title is empty ──────────
    void buildRow_ebayTitleFallsBackToTitle() {
        ComicEntry e;
        e.title      = "Hulk";
        e.ebay_title = "";
        QString row  = CsvHelper::buildEbayRow(e);
        QVERIFY(row.contains("\"Hulk\""));
    }

    // ── Description falls back to notes when ebay_description is empty ───
    void buildRow_descriptionFallsBackToNotes() {
        ComicEntry e;
        e.notes            = "Some raw OCR text.";
        e.ebay_description = "";
        QString row        = CsvHelper::buildEbayRow(e);
        QVERIFY(row.contains("Some raw OCR text."));
    }

    // ── CSV file is written as UTF-8 ─────────────────────────────────────
    void writeEbayCsv_utf8Encoding() {
        QTemporaryFile tmpFile;
        QVERIFY(tmpFile.open());

        QTextStream out(&tmpFile);
        out.setEncoding(QStringConverter::Utf8);

        ComicEntry e;
        e.title       = QString::fromUtf8("Detective Comics – Édition Française");
        e.ebay_title  = e.title;
        e.sku         = "DC-FR-001";

        CsvHelper::writeEbayCsv({e}, out);
        out.flush();
        tmpFile.seek(0);

        QByteArray raw = tmpFile.readAll();
        // UTF-8 BOM is optional; just verify the non-ASCII character encoded correctly
        QVERIFY(raw.contains("\xc3\xa9")); // é in UTF-8
    }

    // ── writeEbayCsv writes header + one data row ────────────────────────
    void writeEbayCsv_headerPlusOneRow() {
        QTemporaryFile tmpFile;
        QVERIFY(tmpFile.open());
        QTextStream out(&tmpFile);
        out.setEncoding(QStringConverter::Utf8);

        ComicEntry e;
        e.title      = "Thor";
        e.sku        = "THOR-001";
        e.ebay_title = "Thor #1";

        CsvHelper::writeEbayCsv({e}, out);
        out.flush();
        tmpFile.seek(0);

        QStringList lines;
        QTextStream in(&tmpFile);
        while (!in.atEnd())
            lines << in.readLine();

        QCOMPARE(lines.size(), 2); // header + 1 data row
        QVERIFY(lines[0].contains("Action"));
        QVERIFY(lines[1].contains("Thor"));
    }

    // ── writeEbayCsv writes correct count for batch of 5 ─────────────────
    void writeEbayCsv_batchOf5() {
        QTemporaryFile tmpFile;
        QVERIFY(tmpFile.open());
        QTextStream out(&tmpFile);
        out.setEncoding(QStringConverter::Utf8);

        QList<ComicEntry> batch;
        for (int i = 0; i < 5; ++i) {
            ComicEntry e;
            e.title = "Comic " + QString::number(i);
            e.sku   = "SKU-" + QString::number(i);
            batch << e;
        }

        CsvHelper::writeEbayCsv(batch, out);
        out.flush();
        tmpFile.seek(0);

        int lineCount = 0;
        QTextStream in(&tmpFile);
        while (!in.atEnd()) { in.readLine(); ++lineCount; }

        QCOMPARE(lineCount, 6); // 1 header + 5 data rows
    }

    // ── UNKNOWN title is preserved verbatim in CSV ────────────────────────
    void buildRow_unknownTitlePreserved() {
        ComicEntry e;
        e.title      = "Unknown";
        e.ebay_title = "Unknown";
        e.sku        = "UNK-001";
        QString row  = CsvHelper::buildEbayRow(e);
        QVERIFY(row.contains("\"Unknown\""));
    }

    // ── SKU is non-empty in exported row ─────────────────────────────────
    void buildRow_skuNonEmpty() {
        ComicEntry e;
        e.title = "Green Lantern";
        e.sku   = "GL-001-ABC";
        QString row = CsvHelper::buildEbayRow(e);
        QVERIFY(row.contains("\"GL-001-ABC\""));
    }
};

QTEST_MAIN(TestEbayCsv)
#include "test_ebaycsv.moc"
