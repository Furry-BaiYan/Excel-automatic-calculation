#pragma once
#include <QString>
#include <QVariant>
#include <QVector>
#include <functional>
#include "DataTable.h"

// 条件规则
struct ConditionRule {
    double   threshold;
    QString  op;       // ">=" "<=" ">" "<" "==" "!="
    QVariant result;

    bool operator==(const ConditionRule& o) const {
        return threshold == o.threshold && op == o.op
               && result == o.result;
    }
};

// 公式描述：列名 + 计算函数
// calc 参数：(当前表引用, 当前行索引) → 返回计算值
struct Formula {
    QString    name;                    // 新列名
    ColumnType type = ColumnType::Text; // 输出类型

    using CalcFn = std::function<QVariant(const DataTable&, int rowIdx)>;
    CalcFn calc;
};

class FormulaEngine {
public:
    // 对整张表应用公式，返回新增列后的 DataTable
    static DataTable apply(const DataTable& table,
                           const QVector<Formula>& formulas);

    // ── 内置公式构造器 ────────────────────────────

    // colA / colB，除零返回 fallback
    static Formula divide(const QString& newCol,
                          const QString& colA,
                          const QString& colB,
                          double fallback = 0.0);

    // colA - colB
    static Formula subtract(const QString& newCol,
                            const QString& colA,
                            const QString& colB);

    // colA + colB
    static Formula add(const QString& newCol,
                       const QString& colA,
                       const QString& colB);

    // colA * colB
    static Formula multiply(const QString& newCol,
                            const QString& colA,
                            const QString& colB);

    // 条件分级：按 srcCol 的数值匹配 rules，返回文本标签
    static Formula classify(const QString& newCol,
                            const QString& srcCol,
                            const QVector<ConditionRule>& rules,
                            const QVariant& defaultVal = QString("未知"));

    // 自定义 lambda
    static Formula custom(const QString& newCol,
                          ColumnType type,
                          Formula::CalcFn fn);

private:
    static bool   _eval(double val, const QString& op, double threshold);
    static double _getDouble(const DataTable& t, int row, const QString& col);
};