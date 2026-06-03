#include "FormulaEngine.h"
#include <QDebug>
#include <cmath>

// ─────────────────────────────────────────────
//  工具：取某行某列的 double 值
// ─────────────────────────────────────────────
double FormulaEngine::_getDouble(const DataTable& t, int row,
                                  const QString& col) {
    // 先判断是否是数字常量（如 "3" "0.5"）
    bool isConst = false;
    double constVal = col.trimmed().toDouble(&isConst);
    if (isConst) return constVal;

    // 否则按列名查找
    int idx = t.columnIndex(col);
    if (idx < 0) return 0.0;
    bool ok = false;
    double d = t.value(row, idx).toString().toDouble(&ok);
    return ok ? d : 0.0;
}

// ─────────────────────────────────────────────
//  工具：比较运算
// ─────────────────────────────────────────────
bool FormulaEngine::_eval(double val, const QString& op, double threshold) {
    if (op == ">=") return val >= threshold;
    if (op == "<=") return val <= threshold;
    if (op == ">" ) return val >  threshold;
    if (op == "<" ) return val <  threshold;
    if (op == "==") return qFuzzyCompare(val + 1.0, threshold + 1.0);
    if (op == "!=") return !qFuzzyCompare(val + 1.0, threshold + 1.0);
    return false;
}

// ─────────────────────────────────────────────
//  核心：应用公式列表
// ─────────────────────────────────────────────
DataTable FormulaEngine::apply(const DataTable& table,
                                const QVector<Formula>& formulas) {
    DataTable result = table;

    for (const auto& formula : formulas) {
        // 检查列是否已存在
        int existIdx = result.columnIndex(formula.name);

        if (existIdx < 0) {
            // 不存在 → 追加新列
            ColumnMeta meta;
            meta.name  = formula.name;
            meta.type  = formula.type;
            meta.index = result.columns.size();
            result.columns << meta;

            for (int ri = 0; ri < result.rows.size(); ++ri) {
                QVariant val = formula.calc ? formula.calc(result, ri) : QVariant();
                result.rows[ri] << val;
            }
        } else {
            // 已存在 → 原地覆盖
            result.columns[existIdx].type = formula.type;

            for (int ri = 0; ri < result.rows.size(); ++ri) {
                QVariant val = formula.calc ? formula.calc(result, ri) : QVariant();
                result.rows[ri][existIdx] = val;
            }
        }

        qDebug() << "[FormulaEngine] 完成列:" << formula.name;
    }

    return result;
}

// ─────────────────────────────────────────────
//  内置：除法
// ─────────────────────────────────────────────
Formula FormulaEngine::divide(const QString& newCol,
                               const QString& colA,
                               const QString& colB,
                               double fallback) {
    Formula f;
    f.name = newCol;
    f.type = ColumnType::Double;
    f.calc = [colA, colB, fallback](const DataTable& t, int ri) -> QVariant {
        double a = FormulaEngine::_getDouble(t, ri, colA);
        double b = FormulaEngine::_getDouble(t, ri, colB);
        if (qFuzzyIsNull(b)) return fallback;
        double r = a / b;
        // 保留两位小数
        return QString::number(r, 'f', 2).toDouble();
    };
    return f;
}

// ─────────────────────────────────────────────
//  内置：减法
// ─────────────────────────────────────────────
Formula FormulaEngine::subtract(const QString& newCol,
                                 const QString& colA,
                                 const QString& colB) {
    Formula f;
    f.name = newCol;
    f.type = ColumnType::Double;
    f.calc = [colA, colB](const DataTable& t, int ri) -> QVariant {
        double a = FormulaEngine::_getDouble(t, ri, colA);
        double b = FormulaEngine::_getDouble(t, ri, colB);
        return a - b;
    };
    return f;
}

// ─────────────────────────────────────────────
//  内置：加法
// ─────────────────────────────────────────────
Formula FormulaEngine::add(const QString& newCol,
                            const QString& colA,
                            const QString& colB) {
    Formula f;
    f.name = newCol;
    f.type = ColumnType::Double;
    f.calc = [colA, colB](const DataTable& t, int ri) -> QVariant {
        double a = FormulaEngine::_getDouble(t, ri, colA);
        double b = FormulaEngine::_getDouble(t, ri, colB);
        return a + b;
    };
    return f;
}

// ─────────────────────────────────────────────
//  内置：乘法
// ─────────────────────────────────────────────
Formula FormulaEngine::multiply(const QString& newCol,
                                 const QString& colA,
                                 const QString& colB) {
    Formula f;
    f.name = newCol;
    f.type = ColumnType::Double;
    f.calc = [colA, colB](const DataTable& t, int ri) -> QVariant {
        double a = FormulaEngine::_getDouble(t, ri, colA);
        double b = FormulaEngine::_getDouble(t, ri, colB);
        return a * b;
    };
    return f;
}

// ─────────────────────────────────────────────
//  内置：条件分级
// ─────────────────────────────────────────────
Formula FormulaEngine::classify(const QString& newCol,
                                 const QString& srcCol,
                                 const QVector<ConditionRule>& rules,
                                 const QVariant& defaultVal) {
    Formula f;
    f.name = newCol;
    f.type = ColumnType::Text;
    f.calc = [srcCol, rules, defaultVal](const DataTable& t, int ri) -> QVariant {
        double val = FormulaEngine::_getDouble(t, ri, srcCol);
        for (const auto& rule : rules) {
            if (FormulaEngine::_eval(val, rule.op, rule.threshold))
                return rule.result;
        }
        return defaultVal;
    };
    return f;
}

// ─────────────────────────────────────────────
//  自定义 lambda
// ─────────────────────────────────────────────
Formula FormulaEngine::custom(const QString& newCol,
                               ColumnType type,
                               Formula::CalcFn fn) {
    Formula f;
    f.name = newCol;
    f.type = type;
    f.calc = fn;
    return f;
}