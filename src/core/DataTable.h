#pragma once
#include <QString>
#include <QVariant>
#include <QVector>
#include <QStringList>

// 列类型
enum class ColumnType {
    Unknown,
    Text,
    Integer,
    Double,
    Date,
    DateTime,
    Boolean
};

// 单列的元信息
struct ColumnMeta {
    QString    name;                    // 列名（表头）
    ColumnType type = ColumnType::Unknown;
    int        index = 0;              // 列索引（0起）
    bool       hasHeader = true;

    QString typeString() const {
        switch (type) {
            case ColumnType::Text:     return "文本";
            case ColumnType::Integer:  return "整数";
            case ColumnType::Double:   return "小数";
            case ColumnType::Date:     return "日期";
            case ColumnType::DateTime: return "日期时间";
            case ColumnType::Boolean:  return "布尔";
            default:                   return "未知";
        }
    }
};

// 核心数据表：行列结构
class DataTable {
public:
    QString              name;       // 表名（来自文件名或Sheet名）
    QString              sourcePath; // 源文件路径
    QString              sheetName;  // Sheet名
    int                  headerRow = 0; // 表头所在行（0起）
    int                  dataStartRow = 1;

    QVector<ColumnMeta>        columns;  // 列元信息
    QVector<QVector<QVariant>> rows;     // 数据行

    // --- 基本操作 ---

    int rowCount()    const { return rows.size(); }
    int columnCount() const { return columns.size(); }
    bool isEmpty()    const { return rows.isEmpty(); }

    // 按列名获取列索引，找不到返回 -1
    int columnIndex(const QString& name) const {
        for (int i = 0; i < columns.size(); ++i)
            if (columns[i].name == name) return i;
        return -1;
    }

    // 获取某行某列的值
    QVariant value(int row, int col) const {
        if (row < 0 || row >= rows.size()) return {};
        if (col < 0 || col >= rows[row].size()) return {};
        return rows[row][col];
    }

    QVariant value(int row, const QString& colName) const {
        return value(row, columnIndex(colName));
    }

    // 获取所有列名
    QStringList columnNames() const {
        QStringList names;
        for (const auto& c : columns)
            names << c.name;
        return names;
    }

    // 打印摘要（调试用）
    QString summary() const {
        return QString("表: %1 | %2 行 x %3 列 | 来源: %4")
               .arg(name).arg(rowCount()).arg(columnCount()).arg(sourcePath);
    }
};