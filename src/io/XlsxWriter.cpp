#include "XlsxWriter.h"
#include "xlsxdocument.h"
#include "xlsxformat.h"
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include <QDebug>

// ─────────────────────────────────────────────
//  工具：构建表头格式
// ─────────────────────────────────────────────
static QXlsx::Format makeHeaderFormat(const WriteConfig& cfg) {
    QXlsx::Format fmt;
    fmt.setFontBold(cfg.boldHeader);
    fmt.setFontColor(cfg.headerFont);
    fmt.setPatternBackgroundColor(cfg.headerBg);
    fmt.setHorizontalAlignment(QXlsx::Format::AlignHCenter);
    fmt.setVerticalAlignment(QXlsx::Format::AlignVCenter);
    return fmt;
}

// ─────────────────────────────────────────────
//  工具：构建数据格式（按列类型 + 颜色规则）
// ─────────────────────────────────────────────
static QXlsx::Format makeDataFormat(const ColumnMeta& col,
                                     const QVariant& value,
                                     const WriteConfig& cfg) {
    QXlsx::Format fmt;
    fmt.setVerticalAlignment(QXlsx::Format::AlignVCenter);

    // 数字格式
    if (col.type == ColumnType::Double)
        fmt.setNumberFormat("0.00");
    else if (col.type == ColumnType::Integer)
        fmt.setNumberFormat("0");

    // 值→背景色规则
    for (const auto& rule : cfg.colorRules) {
        if (rule.colName == col.name) {
            QString val = value.toString();
            if (rule.colorMap.contains(val)) {
                QColor bg = rule.colorMap[val];
                fmt.setPatternBackgroundColor(bg);
                // 深色背景用白字，浅色用黑字
                int luma = bg.red()*299 + bg.green()*587 + bg.blue()*114;
                fmt.setFontColor(luma > 128000 ? Qt::black : Qt::white);
            }
        }
    }

    return fmt;
}

// ─────────────────────────────────────────────
//  核心写入函数（写一张 Sheet）
// ─────────────────────────────────────────────
static bool writeSheet(QXlsx::Document& doc,
                        const DataTable& table,
                        const WriteConfig& cfg,
                        QString* errorMsg) {
    if (table.columns.isEmpty()) {
        if (errorMsg) *errorMsg = "DataTable 没有列信息";
        return false;
    }

    QXlsx::Format headerFmt = makeHeaderFormat(cfg);

    // ── 写表头（第1行）──
    for (int ci = 0; ci < table.columns.size(); ++ci) {
        doc.write(1, ci + 1, table.columns[ci].name, headerFmt);
    }

    // ── 写数据行（从第2行起）──
    for (int ri = 0; ri < table.rowCount(); ++ri) {
        for (int ci = 0; ci < table.columnCount(); ++ci) {
            QVariant val = table.value(ri, ci);
            const ColumnMeta& col = table.columns[ci];
            QXlsx::Format fmt = makeDataFormat(col, val, cfg);

            // 数值类型直接写数字，文本写字符串
            if ((col.type == ColumnType::Integer ||
                 col.type == ColumnType::Double) && !val.isNull()) {
                bool ok = false;
                double d = val.toString().toDouble(&ok);
                if (ok)
                    doc.write(ri + 2, ci + 1, d, fmt);
                else
                    doc.write(ri + 2, ci + 1, val.toString(), fmt);
            } else {
                doc.write(ri + 2, ci + 1, val.toString(), fmt);
            }
        }
    }

    // ── 自动列宽 ──
    if (cfg.autoColWidth) {
        for (int ci = 0; ci < table.columnCount(); ++ci) {
            // 估算最大内容宽度
            int maxLen = table.columns[ci].name.length();
            for (int ri = 0; ri < table.rowCount(); ++ri) {
                int len = table.value(ri, ci).toString().length();
                if (len > maxLen) maxLen = len;
            }
            // 中文字符约占2个英文宽度，给一点余量
            double colWidth = maxLen * 1.5 + 2.0;
            if (colWidth < 8)  colWidth = 8;
            if (colWidth > 40) colWidth = 40;
            doc.setColumnWidth(ci + 1, colWidth);
        }
    }

    // ── 冻结首行 ──
    doc.setRowHeight(1, 22);

    return true;
}

// ─────────────────────────────────────────────
//  公开：写单张表
// ─────────────────────────────────────────────
bool XlsxWriter::write(const DataTable& table,
                        const QString& filePath,
                        const WriteConfig& cfg,
                        QString* errorMsg) {
    // 确保输出目录存在
    QDir().mkpath(QFileInfo(filePath).absolutePath());

    QXlsx::Document doc;

    // 设置 Sheet 名称
    QString sheetName = table.name.isEmpty() ? "Sheet1" : table.name;
    // xlsx Sheet 名不能超过31字符且不能有特殊符号
    sheetName = sheetName.left(31).replace(QRegularExpression("[\\\\/:*?\\[\\]]"), "_");
    doc.renameSheet("Sheet1", sheetName);

    if (!writeSheet(doc, table, cfg, errorMsg)) return false;

    bool ok = doc.saveAs(filePath);
    if (!ok && errorMsg) *errorMsg = "保存失败: " + filePath;

    qDebug() << "[XlsxWriter]" << (ok ? "写入成功" : "写入失败") << filePath;
    return ok;
}

// ─────────────────────────────────────────────
//  公开：写多张表到不同 Sheet
// ─────────────────────────────────────────────
bool XlsxWriter::writeSheets(const QVector<DataTable>& tables,
                               const QString& filePath,
                               const WriteConfig& cfg,
                               QString* errorMsg) {
    if (tables.isEmpty()) {
        if (errorMsg) *errorMsg = "没有数据";
        return false;
    }

    QDir().mkpath(QFileInfo(filePath).absolutePath());
    QXlsx::Document doc;

    for (int i = 0; i < tables.size(); ++i) {
        const DataTable& t = tables[i];
        QString sheetName = t.name.isEmpty()
                            ? QString("Sheet%1").arg(i + 1)
                            : t.name.left(31);
        sheetName.replace(QRegularExpression("[\\\\/:*?\\[\\]]"), "_");

        if (i == 0)
            doc.renameSheet("Sheet1", sheetName);
        else
            doc.addSheet(sheetName);

        doc.selectSheet(sheetName);

        QString err;
        if (!writeSheet(doc, t, cfg, &err)) {
            if (errorMsg) *errorMsg = err;
            return false;
        }
    }

    bool ok = doc.saveAs(filePath);
    if (!ok && errorMsg) *errorMsg = "保存失败: " + filePath;
    qDebug() << "[XlsxWriter] 多Sheet写入" << (ok ? "成功" : "失败") << filePath;
    return ok;
}