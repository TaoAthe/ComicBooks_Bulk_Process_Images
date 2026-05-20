#pragma once

#include <QByteArray>
#include <QJsonArray>

// Generates an eBay bulk-listing CSV from a JSON array of comic records.
// The output matches the eBay File Exchange / Seller Hub CSV format so the
// file can be imported directly without further editing.
class EbayCsvExporter {
public:
    static QByteArray generate(const QJsonArray &comics);

private:
    static QString field(const QString &value);   // quote + escape one CSV cell
    static int     conditionId(const QString &condition);
    static QString buildSku(int id, const QString &title, const QString &issue);
    static QString buildDescription(const QString &title, const QString &issue,
                                    const QString &year, const QString &publisher,
                                    const QString &condition, const QString &notes);
};
