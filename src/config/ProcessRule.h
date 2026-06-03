#pragma once
#include <QString>
#include <QStringList>
#include <QVector>
#include "../core/FormulaEngine.h"   // ConditionRule

// ── 一条公式规则 ───────────────────────────────
struct FormulaRule {
    QString name;    // 新列名
    QString type;    // "divide" "subtract" "add" "multiply" "classify"
    QString colA;    // 操作列A（算术）或来源列（分级）
    QString colB;    // 操作列B（算术用）
    QVector<ConditionRule> conditions; // 分级条件（classify用）
};

// ── 统计配置 ───────────────────────────────────
struct StatRule {
    QStringList statCols;   // 要统计的数值列
    QString     groupCol;   // 分组列（空=不分组）
};

// ── 完整处理规则 ───────────────────────────────
struct ProcessRule {
    QVector<FormulaRule> formulas;
    StatRule             stat;

    bool isEmpty() const { return formulas.isEmpty(); }

    // JSON 序列化
    bool saveToFile(const QString& path, QString* err = nullptr) const;
    static ProcessRule loadFromFile(const QString& path, QString* err = nullptr);
};