#include "ebayexporter.h"

#include <QJsonObject>
#include <QHash>
#include <QStringList>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Wrap a value in double-quotes and escape any embedded double-quotes.
QString EbayCsvExporter::field(const QString &value) {
    QString escaped = value;
    escaped.replace(QLatin1Char('"'), QLatin1String("\"\""));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

// Map human-readable condition to eBay ConditionID.
int EbayCsvExporter::conditionId(const QString &condition) {
    static const QHash<QString, int> kMap = {
        {QStringLiteral("near mint"),  2000},
        {QStringLiteral("very fine"),  3000},
        {QStringLiteral("fine"),       3000},
        {QStringLiteral("very good"),  4000},
        {QStringLiteral("good"),       5000},
        {QStringLiteral("fair"),       5000},
        {QStringLiteral("poor"),       7000},
    };
    return kMap.value(condition.toLower().trimmed(), 4000 /* Very Good default */);
}

// Build a clean SKU from available fields.
QString EbayCsvExporter::buildSku(int id, const QString &title, const QString &issue) {
    QString raw = title + QStringLiteral("-") + issue + QStringLiteral("-") + QString::number(id);
    QString sku;
    sku.reserve(raw.size());
    for (const QChar &c : raw) {
        if (c.isLetterOrNumber()) {
            sku += c.toUpper();
        } else if (!sku.isEmpty() && sku.back() != QLatin1Char('-')) {
            sku += QLatin1Char('-');
        }
    }
    // Remove trailing dash and enforce eBay's 50-char custom-label limit.
    while (!sku.isEmpty() && sku.back() == QLatin1Char('-')) {
        sku.chop(1);
    }
    return sku.left(50);
}

// Build a simple HTML description block.
QString EbayCsvExporter::buildDescription(const QString &title, const QString &issue,
                                          const QString &year, const QString &publisher,
                                          const QString &condition, const QString &notes) {
    QString html;
    html.reserve(512);
    html += QStringLiteral("<p><strong>");
    html += title;
    if (!issue.isEmpty()) { html += QStringLiteral(" #"); html += issue; }
    if (!year.isEmpty())  { html += QStringLiteral(" ("); html += year; html += QLatin1Char(')'); }
    html += QStringLiteral("</strong></p>");
    if (!publisher.isEmpty()) {
        html += QStringLiteral("<p>Publisher: "); html += publisher; html += QStringLiteral("</p>");
    }
    if (!condition.isEmpty()) {
        html += QStringLiteral("<p>Condition: "); html += condition; html += QStringLiteral("</p>");
    }
    if (!notes.isEmpty()) {
        html += QStringLiteral("<p>"); html += notes; html += QStringLiteral("</p>");
    }
    html += QStringLiteral("<p>Shipped with care in protective bag and board.</p>");
    return html;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

QByteArray EbayCsvExporter::generate(const QJsonArray &comics) {
    // Column definitions — only the subset we can populate from our DB.
    // The first column name must carry the eBay metadata exactly as shown.
    static const QStringList kHeaders = {
        QStringLiteral("*Action(SiteID=US|Country=US|Currency=USD|Version=1193|CC=UTF-8)"),
        QStringLiteral("CustomLabel"),
        QStringLiteral("*Category"),
        QStringLiteral("*Title"),
        QStringLiteral("*ConditionID"),
        QStringLiteral("C:Publication Name"),
        QStringLiteral("C:Publisher"),
        QStringLiteral("C:Publication Year"),
        QStringLiteral("C:Issue Number"),
        QStringLiteral("*Description"),
        QStringLiteral("*Format"),
        QStringLiteral("*Duration"),
        QStringLiteral("*StartPrice"),
        QStringLiteral("*Quantity"),
        QStringLiteral("*Location"),
    };

    QString out;
    out.reserve(comics.size() * 300 + 512);

    // Row 1: eBay template metadata (required by Seller Hub importer).
    out += QStringLiteral("Info,Version=1.0.0,Template=fx_category_template_EBAY_US\n");

    // Row 2: column headers.
    QStringList headerCells;
    headerCells.reserve(kHeaders.size());
    for (const QString &h : kHeaders) {
        headerCells << field(h);
    }
    out += headerCells.join(QLatin1Char(',')) + QLatin1Char('\n');

    // Data rows.
    for (const QJsonValue &val : comics) {
        const QJsonObject comic = val.toObject();
        const int    id        = comic.value(QStringLiteral("id")).toInt();
        const QString title     = comic.value(QStringLiteral("title")).toString();
        const QString issue     = comic.value(QStringLiteral("issue")).toString();
        const QString year      = comic.value(QStringLiteral("year")).toString();
        const QString publisher = comic.value(QStringLiteral("publisher")).toString();
        const QString condition = comic.value(QStringLiteral("condition")).toString();
        const QString notes     = comic.value(QStringLiteral("notes")).toString();
        const QString value     = comic.value(QStringLiteral("value")).toString();

        // eBay title: 80-char hard limit.
        QString ebayTitle = title;
        if (!issue.isEmpty())     { ebayTitle += QStringLiteral(" #"); ebayTitle += issue; }
        if (!year.isEmpty())      { ebayTitle += QLatin1Char(' '); ebayTitle += year; }
        if (!publisher.isEmpty()) { ebayTitle += QLatin1Char(' '); ebayTitle += publisher; }
        ebayTitle = ebayTitle.left(80);

        // Price: strip currency symbols; fall back to a placeholder.
        QString price = value;
        price.remove(QLatin1Char('$')).remove(QLatin1Char(',')).remove(QLatin1Char(' '));
        bool priceOk = false;
        const double priceVal = price.toDouble(&priceOk);
        if (!priceOk || priceVal <= 0.0) {
            price = QStringLiteral("9.99");
        }

        QStringList row;
        row.reserve(kHeaders.size());
        row << field(QStringLiteral("Add"));
        row << field(buildSku(id, title, issue));
        row << field(QStringLiteral("63")); // eBay category: Comics > Single Issues
        row << field(ebayTitle);
        row << field(QString::number(conditionId(condition)));
        row << field(title);
        row << field(publisher);
        row << field(year);
        row << field(issue);
        row << field(buildDescription(title, issue, year, publisher, condition, notes));
        row << field(QStringLiteral("FixedPriceItem"));
        row << field(QStringLiteral("GTC"));
        row << field(price);
        row << field(QStringLiteral("1"));
        row << field(QStringLiteral("United States"));

        out += row.join(QLatin1Char(',')) + QLatin1Char('\n');
    }

    return out.toUtf8();
}
