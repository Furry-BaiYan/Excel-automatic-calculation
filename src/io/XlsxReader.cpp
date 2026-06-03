#include "XlsxReader.h"
#include "xlsxdocument.h"
#include "xlsxworksheet.h"
#include <QDir>
#include <QFileInfo>
#include <QDate>
#include <QDateTime>
#include <QDebug>

// ─────────────────────────────────────────────
//  工具：判断一行是否像"表头"
// ─────────────────────────────────────────────
bool XlsxReader::_isHeaderRow(const QVector<QVariant>& row) {
    if (row.isEmpty()) return false;
    int textCount = 0, emptyCount = 0;
    for (const auto& v : row) {
        if (!v.isValid() || v.isNull() || v.toString().trimmed().isEmpty()) {
            emptyCount++;
            continue;
        }
        bool isNum = false;
        v.toString().toDouble(&isNum);
        if (isNum) return false;
        textCount++;
    }
    return textCount > (row.size() - emptyCount) / 2;
}

// ─────────────────────────────────────────────
//  工具：推断列类型（Qt6 用 typeId()）
// ─────────────────────────────────────────────
ColumnType XlsxReader::_inferType(const QVector<QVariant>& samples) {
    int intCount=0, dblCount=0, dateCount=0,
        dtCount=0,  boolCount=0, total=0;

    for (const auto& v : samples) {
        if (!v.isValid() || v.isNull()) continue;
        total++;

        // Qt6: 用 typeId() 代替 type()
        int tid = v.typeId();
        if (tid == QMetaType::Bool)      { boolCount++; continue; }
        if (tid == QMetaType::QDate)     { dateCount++; continue; }
        if (tid == QMetaType::QDateTime) { dtCount++;   continue; }

        QString s = v.toString().trimmed();
        bool ok = false;

        s.toLongLong(&ok);
        if (ok) { intCount++; continue; }

        s.toDouble(&ok);
        if (ok) { dblCount++; continue; }

        QDate d = QDate::fromString(s, "yyyy/MM/dd");
        if (!d.isValid()) d = QDate::fromString(s, "yyyy-MM-dd");
        if (d.isValid()) { dateCount++; continue; }
    }

    if (total == 0) return ColumnType::Unknown;
    double r = 0.8;
    if (boolCount  >= total * r) return ColumnType::Boolean;
    if (dtCount    >= total * r) return ColumnType::DateTime;
    if (dateCount  >= total * r) return ColumnType::Date;
    if (intCount   >= total * r) return ColumnType::Integer;
    if ((intCount + dblCount) >= total * r) return ColumnType::Double;
    return ColumnType::Text;
}

// ─────────────────────────────────────────────
//  找表头行
// ─────────────────────────────────────────────
int XlsxReader::_findHeaderRow(const QString& filePath, int maxScan) {
    QXlsx::Document doc(filePath);
    for (int row = 1; row <= maxScan; ++row) {
        QVector<QVariant> rowData;
        for (int col = 1; col <= 20; ++col) {
            // 修复：shared_ptr 用 auto，不用 auto*
            auto cell = doc.cellAt(row, col);
            rowData << (cell ? cell->value() : QVariant());
        }
        while (!rowData.isEmpty() &&
               (!rowData.last().isValid() || rowData.last().isNull()))
            rowData.removeLast();

        if (rowData.isEmpty()) continue;
        if (_isHeaderRow(rowData)) return row - 1;
    }
    return 0;
}

// ─────────────────────────────────────────────
//  探测文件结构
// ─────────────────────────────────────────────
DetectResult XlsxReader::_detect(const QString& filePath,
                                  const ReadConfig& config) {
    DetectResult result;

    if (!QFileInfo::exists(filePath)) {
        result.errorMsg = "文件不存在: " + filePath;
        return result;
    }

    QXlsx::Document doc(filePath);

    int hRow = _findHeaderRow(filePath, config.maxScanRows);
    result.headerRow  = hRow;
    result.dataStart  = hRow + 1;

    // 读表头列名
    QVector<QVariant> headerCells;
    for (int col = 1; col <= 100; ++col) {
        auto cell = doc.cellAt(hRow + 1, col);
        QVariant v = cell ? cell->value() : QVariant();
        if (!v.isValid() || v.isNull()) {
            auto next = doc.cellAt(hRow + 1, col + 1);
            QVariant nv = next ? next->value() : QVariant();
            if (!nv.isValid() || nv.isNull()) break;
            headerCells << v;
        } else {
            headerCells << v;
        }
    }

    result.colCount = headerCells.size();

    for (int i = 0; i < headerCells.size(); ++i) {
        QString name = headerCells[i].toString().trimmed();
        if (name.isEmpty())
            name = QString("列%1").arg(QChar('A' + i));
        result.headers << name;
    }

    // 采样推断类型
    for (int ci = 0; ci < result.colCount; ++ci) {
        QVector<QVariant> samples;
        for (int ri = result.dataStart + 1;
             ri <= result.dataStart + 20; ++ri) {
            auto cell = doc.cellAt(ri, ci + 1);
            if (cell) samples << cell->value();
        }
        result.types << _inferType(samples);
    }

    result.success = true;
    return result;
}

// ─────────────────────────────────────────────
//  公开：仅探测结构
// ─────────────────────────────────────────────
DetectResult XlsxReader::detectStructure(const QString& filePath,
                                          const ReadConfig& config) {
    return _detect(filePath, config);
}

// ─────────────────────────────────────────────
//  公开：读取单个文件
// ─────────────────────────────────────────────
DataTable XlsxReader::readFile(const QString& filePath,
                                const ReadConfig& config,
                                QString* errorMsg) {
    DataTable table;
    table.sourcePath = filePath;
    table.name = QFileInfo(filePath).baseName();

    if (!QFileInfo::exists(filePath)) {
        if (errorMsg) *errorMsg = "文件不存在: " + filePath;
        return table;
    }

    DetectResult det = _detect(filePath, config);
    if (!det.success) {
        if (errorMsg) *errorMsg = det.errorMsg;
        return table;
    }

    table.headerRow    = det.headerRow;
    table.dataStartRow = det.dataStart;

    for (int i = 0; i < det.colCount; ++i) {
        ColumnMeta meta;
        meta.name  = det.headers[i];
        meta.type  = det.types.value(i, ColumnType::Text);
        meta.index = i;
        table.columns << meta;
    }

    QXlsx::Document doc(filePath);
    int maxRow = (config.maxRows > 0)
                 ? det.dataStart + config.maxRows
                 : 100000;

    for (int ri = det.dataStart + 1; ri <= maxRow; ++ri) {
        bool rowEmpty = true;
        QVector<QVariant> rowData;
        for (int ci = 0; ci < det.colCount; ++ci) {
            auto cell = doc.cellAt(ri, ci + 1);
            QVariant v = cell ? cell->value() : QVariant();
            rowData << v;
            if (v.isValid() && !v.isNull() &&
                !v.toString().trimmed().isEmpty())
                rowEmpty = false;
        }
        if (rowEmpty) break;
        table.rows << rowData;
    }

    qDebug() << "[XlsxReader]" << table.summary();
    return table;
}

// ─────────────────────────────────────────────
//  公开：批量读取文件列表
// ─────────────────────────────────────────────
QVector<DataTable> XlsxReader::readFiles(const QStringList& filePaths,
                                          const ReadConfig& config,
                                          ProgressCallback onProgress) {
    QVector<DataTable> result;
    int total = filePaths.size();
    for (int i = 0; i < total; ++i) {
        if (onProgress) onProgress(i + 1, total, filePaths[i]);
        QString err;
        DataTable t = readFile(filePaths[i], config, &err);
        if (!err.isEmpty())
            qWarning() << "[XlsxReader] 跳过:" << filePaths[i] << err;
        else
            result << t;
    }
    return result;
}

// ─────────────────────────────────────────────
//  公开：批量读取文件夹
// ─────────────────────────────────────────────
QVector<DataTable> XlsxReader::readFolder(const QString& folderPath,
                                           const ReadConfig& config,
                                           ProgressCallback onProgress) {
    QDir dir(folderPath);
    QStringList files = dir.entryList({"*.xlsx"}, QDir::Files);
    QStringList fullPaths;
    for (const auto& f : files)
        fullPaths << dir.absoluteFilePath(f);

    if (fullPaths.isEmpty()) {
        qWarning() << "[XlsxReader] 没有找到 xlsx 文件:" << folderPath;
        return {};
    }
    return readFiles(fullPaths, config, onProgress);
}
