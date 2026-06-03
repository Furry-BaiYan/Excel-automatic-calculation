#pragma once
#include <QString>
#include <QColor>
#include <QMap>
#include "../core/DataTable.h"

// 列样式配置
struct ColStyle {
    bool        bold       = false;
    QColor      bgColor;          // 背景色，无效=不设置
    QColor      fontColor;        // 字体色
    QString     numFormat  = "";  // 数字格式，如 "0.00"
    int         width      = -1;  // 列宽，-1=自动
};

// 值→颜色映射（用于绩效评分等字段）
struct ValueColorRule {
    QString colName;                    // 哪一列
    QMap<QString, QColor> colorMap;     // 值 → 背景色
};

// 写入配置
struct WriteConfig {
    bool   boldHeader    = true;    // 表头加粗
    QColor headerBg      = QColor(68, 114, 196);  // 表头背景（深蓝）
    QColor headerFont    = Qt::white;              // 表头字色
    bool   autoColWidth  = true;    // 自动列宽
    bool   addBorder     = false;   // 单元格边框（暂留）

    QVector<ValueColorRule> colorRules;  // 按值着色规则
};

class XlsxWriter {
public:
    // 把 DataTable 写入 xlsx 文件
    static bool write(const DataTable& table,
                      const QString& filePath,
                      const WriteConfig& config = {},
                      QString* errorMsg = nullptr);

    // 把多张 DataTable 写入同一个 xlsx 的不同 Sheet
    static bool writeSheets(const QVector<DataTable>& tables,
                            const QString& filePath,
                            const WriteConfig& config = {},
                            QString* errorMsg = nullptr);
};