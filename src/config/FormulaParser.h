#pragma once
#include <QString>
#include <QStringList>
#include "../core/FormulaEngine.h"

class FormulaParser {
public:
    struct Result {
        QVector<Formula> formulas;
        QStringList      errors;
        bool ok() const { return errors.isEmpty(); }
    };

    // 解析多行公式文本，返回可直接传给 FormulaEngine::apply 的 Formula 列表
    // knownCols: 文件中已有的列名（新生成的列也会自动追加）
    static Result parse(const QString& text, QStringList knownCols = {});
};