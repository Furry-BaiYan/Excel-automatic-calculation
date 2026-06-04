#include "XlsxWriter.h"
#include "xlsxdocument.h"
#include "xlsxformat.h"
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include <QDebug>

// 计算字符串显示宽度（中文算2，英文算1）
static int dispWidth(const QString& s) {
    int w = 0;
    for (const QChar& c : s) w += (c.unicode() > 127) ? 2 : 1;
    return w;
}

static QXlsx::Format makeHeaderFormat(const WriteConfig& cfg) {
    QXlsx::Format fmt;
    fmt.setFontBold(cfg.boldHeader);
    fmt.setFontColor(cfg.headerFont);
    fmt.setPatternBackgroundColor(cfg.headerBg);
    fmt.setHorizontalAlignment(QXlsx::Format::AlignHCenter);
    fmt.setVerticalAlignment(QXlsx::Format::AlignVCenter);
    return fmt;
}

static QXlsx::Format makeDataFormat(const ColumnMeta& col,
                                     const QVariant& value,
                                     const WriteConfig& cfg) {
    QXlsx::Format fmt;
    fmt.setVerticalAlignment(QXlsx::Format::AlignVCenter);
    if (col.type == ColumnType::Double)   fmt.setNumberFormat("0.00");
    else if (col.type == ColumnType::Integer) fmt.setNumberFormat("0");

    for (const auto& rule : cfg.colorRules) {
        if (rule.colName == col.name) {
            QString val = value.toString();
            if (rule.colorMap.contains(val)) {
                QColor bg = rule.colorMap[val];
                fmt.setPatternBackgroundColor(bg);
                int luma = bg.red()*299 + bg.green()*587 + bg.blue()*114;
                fmt.setFontColor(luma > 128000 ? Qt::black : Qt::white);
            }
        }
    }
    return fmt;
}

static bool writeSheet(QXlsx::Document& doc,
                        const DataTable& table,
                        const WriteConfig& cfg,
                        QString* errorMsg) {
    if (table.columns.isEmpty()) {
        if (errorMsg) *errorMsg = "DataTable 没有列信息";
        return false;
    }

    QXlsx::Format headerFmt = makeHeaderFormat(cfg);

    // 写表头
    for (int ci = 0; ci < table.columns.size(); ++ci)
        doc.write(1, ci+1, table.columns[ci].name, headerFmt);

    // 写数据行
    for (int ri = 0; ri < table.rowCount(); ++ri) {
        for (int ci = 0; ci < table.columnCount(); ++ci) {
            QVariant val = table.value(ri, ci);
            const ColumnMeta& col = table.columns[ci];
            QXlsx::Format fmt = makeDataFormat(col, val, cfg);

            if ((col.type == ColumnType::Integer ||
                 col.type == ColumnType::Double) && !val.isNull()) {
                bool ok = false;
                double d = val.toString().toDouble(&ok);
                if (ok) doc.write(ri+2, ci+1, d, fmt);
                else    doc.write(ri+2, ci+1, val.toString(), fmt);
            } else {
                doc.write(ri+2, ci+1, val.toString(), fmt);
            }
        }
    }

    // 自动列宽（中文字符算2个宽度）
    if (cfg.autoColWidth) {
        for (int ci = 0; ci < table.columnCount(); ++ci) {
            int maxW = dispWidth(table.columns[ci].name);
            for (int ri = 0; ri < table.rowCount(); ++ri) {
                int w = dispWidth(table.value(ri, ci).toString());
                if (w > maxW) maxW = w;
            }
            double colWidth = maxW * 1.1 + 2.0;
            if (colWidth < 8)  colWidth = 8;
            if (colWidth > 50) colWidth = 50;
            doc.setColumnWidth(ci+1, colWidth);
        }
    }

    doc.setRowHeight(1, 22);
    return true;
}

bool XlsxWriter::write(const DataTable& table,
                        const QString& filePath,
                        const WriteConfig& cfg,
                        QString* errorMsg) {
    QDir().mkpath(QFileInfo(filePath).absolutePath());
    QXlsx::Document doc;

    QString sheetName = table.name.isEmpty() ? "Sheet1" : table.name;
    sheetName = sheetName.left(31).replace(QRegularExpression("[\\\\/:*?\\[\\]]"), "_");
    doc.renameSheet("Sheet1", sheetName);

    if (!writeSheet(doc, table, cfg, errorMsg)) return false;

    bool ok = doc.saveAs(filePath);
    if (!ok && errorMsg) *errorMsg = "保存失败: " + filePath;
    qDebug() << "[XlsxWriter]" << (ok?"写入成功":"写入失败") << filePath;
    return ok;
}

bool XlsxWriter::writeSheets(const QVector<DataTable>& tables,
                               const QString& filePath,
                               const WriteConfig& cfg,
                               QString* errorMsg) {
    if (tables.isEmpty()) { if (errorMsg) *errorMsg = "没有数据"; return false; }
    QDir().mkpath(QFileInfo(filePath).absolutePath());
    QXlsx::Document doc;

    for (int i = 0; i < tables.size(); ++i) {
        const DataTable& t = tables[i];
        QString sheetName = t.name.isEmpty()
            ? QString("Sheet%1").arg(i+1) : t.name.left(31);
        sheetName.replace(QRegularExpression("[\\\\/:*?\\[\\]]"), "_");

        if (i == 0) doc.renameSheet("Sheet1", sheetName);
        else        doc.addSheet(sheetName);
        doc.selectSheet(sheetName);

        QString err;
        if (!writeSheet(doc, t, cfg, &err)) {
            if (errorMsg) *errorMsg = err;
            return false;
        }
    }

    bool ok = doc.saveAs(filePath);
    if (!ok && errorMsg) *errorMsg = "保存失败: " + filePath;
    qDebug() << "[XlsxWriter] 多Sheet写入" << (ok?"成功":"失败") << filePath;
    return ok;
}