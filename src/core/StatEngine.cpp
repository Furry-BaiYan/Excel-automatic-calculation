#include "StatEngine.h"
#include <QDebug>
#include <limits>

// ─────────────────────────────────────────────
//  单列统计
// ─────────────────────────────────────────────
ColumnStat StatEngine::colStat(const DataTable& table,
                                const QString& colName) {
    ColumnStat s;
    s.name  = colName;
    s.total = table.rowCount();
    s.min   =  std::numeric_limits<double>::max();
    s.max   = -std::numeric_limits<double>::max();

    int colIdx = table.columnIndex(colName);
    if (colIdx < 0) {
        qWarning() << "[StatEngine] 列不存在:" << colName;
        return s;
    }

    for (int ri = 0; ri < table.rowCount(); ++ri) {
        QVariant v = table.value(ri, colIdx);
        if (!v.isValid() || v.isNull()) continue;
        bool ok = false;
        double d = v.toString().toDouble(&ok);
        if (!ok) continue;

        s.sum += d;
        if (d < s.min) s.min = d;
        if (d > s.max) s.max = d;
        s.count++;
    }

    s.mean = (s.count > 0) ? s.sum / s.count : 0.0;

    // 没有有效数值时重置 min/max
    if (s.count == 0) { s.min = 0; s.max = 0; }

    qDebug() << "[StatEngine]" << colName
             << "sum=" << s.sum << "mean=" << s.mean
             << "min=" << s.min << "max=" << s.max
             << "count=" << s.count;
    return s;
}

// ─────────────────────────────────────────────
//  批量统计
// ─────────────────────────────────────────────
QVector<ColumnStat> StatEngine::colStats(const DataTable& table,
                                          const QStringList& colNames) {
    QVector<ColumnStat> result;
    for (const auto& name : colNames)
        result << colStat(table, name);
    return result;
}

// ─────────────────────────────────────────────
//  分组统计：按 groupCol 分组，统计 statCols
//  输出 DataTable：
//    列 = [groupCol, colA_求和, colA_均值, colA_计数, colB_求和, ...]
// ─────────────────────────────────────────────
DataTable StatEngine::groupBy(const DataTable& table,
                               const QString& groupCol,
                               const QStringList& statCols) {
    DataTable result;
    result.name = table.name + "_分组统计";

    int gIdx = table.columnIndex(groupCol);
    if (gIdx < 0) {
        qWarning() << "[StatEngine] 分组列不存在:" << groupCol;
        return result;
    }

    // 1. 分组：收集每个分组值对应的行索引
    QMap<QString, QVector<int>> groups;
    for (int ri = 0; ri < table.rowCount(); ++ri) {
        QString key = table.value(ri, gIdx).toString().trimmed();
        if (key.isEmpty()) key = "(空)";
        groups[key] << ri;
    }

    // 2. 构建结果列
    result.columns << ColumnMeta{groupCol, ColumnType::Text, 0};
    result.columns << ColumnMeta{"数量", ColumnType::Integer, 1};

    int colOffset = 2;
    for (const auto& sc : statCols) {
        result.columns << ColumnMeta{sc + "_合计", ColumnType::Double, colOffset++};
        result.columns << ColumnMeta{sc + "_均值", ColumnType::Double, colOffset++};
        result.columns << ColumnMeta{sc + "_最大", ColumnType::Double, colOffset++};
        result.columns << ColumnMeta{sc + "_最小", ColumnType::Double, colOffset++};
    }

    // 3. 填充每组数据
    for (auto it = groups.begin(); it != groups.end(); ++it) {
        const QString& groupVal = it.key();
        const QVector<int>& rowIdxs = it.value();

        QVector<QVariant> row;
        row << groupVal;
        row << rowIdxs.size();

        for (const auto& sc : statCols) {
            int scIdx = table.columnIndex(sc);
            double sum = 0, mn = std::numeric_limits<double>::max(),
                   mx  = -std::numeric_limits<double>::max();
            int cnt = 0;

            for (int ri : rowIdxs) {
                if (scIdx < 0) continue;
                bool ok = false;
                double d = table.value(ri, scIdx).toString().toDouble(&ok);
                if (!ok) continue;
                sum += d;
                if (d < mn) mn = d;
                if (d > mx) mx = d;
                cnt++;
            }

            double mean = cnt > 0 ? sum / cnt : 0.0;
            if (cnt == 0) { mn = 0; mx = 0; }

            row << QString::number(sum,  'f', 2).toDouble();
            row << QString::number(mean, 'f', 2).toDouble();
            row << QString::number(mx,   'f', 2).toDouble();
            row << QString::number(mn,   'f', 2).toDouble();
        }

        result.rows << row;
    }

    qDebug() << "[StatEngine] 分组完成，共" << groups.size() << "组";
    return result;
}

// ─────────────────────────────────────────────
//  生成完整报告
// ─────────────────────────────────────────────
StatReport StatEngine::report(const DataTable& table,
                               const QStringList& numCols,
                               const QString& groupCol) {
    StatReport rpt;
    rpt.title     = table.name + " 统计报告";
    rpt.colStats  = colStats(table, numCols);
    if (!groupCol.isEmpty())
        rpt.groupTable = groupBy(table, groupCol, numCols);
    return rpt;
}