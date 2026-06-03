#pragma once
#include <QString>
#include <QVariant>
#include <QVector>
#include <QMap>
#include "DataTable.h"

// 单列统计结果
struct ColumnStat {
    QString name;
    double  sum     = 0;
    double  mean    = 0;
    double  min     = 0;
    double  max     = 0;
    int     count   = 0;   // 非空数量
    int     total   = 0;   // 总行数
};

// 分组统计结果：一个分组值 → 该组的行索引
using GroupMap = QMap<QString, QVector<int>>;

// 汇总报告
struct StatReport {
    QString              title;
    QVector<ColumnStat>  colStats;   // 各列统计
    DataTable            groupTable; // 分组汇总表（可选）
};

class StatEngine {
public:
    // 对指定列做基础统计（求和/均值/最大/最小/计数）
    static ColumnStat colStat(const DataTable& table,
                              const QString& colName);

    // 批量统计多列
    static QVector<ColumnStat> colStats(const DataTable& table,
                                        const QStringList& colNames);

    // 按某列分组，统计每组的指定列汇总
    // groupCol:  分组依据列（如"绩效评分"）
    // statCols:  要统计的数值列列表
    // 返回一张汇总 DataTable：行 = 每个分组，列 = 统计指标
    static DataTable groupBy(const DataTable& table,
                             const QString& groupCol,
                             const QStringList& statCols);

    // 生成完整报告（基础统计 + 分组统计）
    static StatReport report(const DataTable& table,
                             const QStringList& numCols,
                             const QString& groupCol = "");
};