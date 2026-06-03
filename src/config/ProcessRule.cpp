#include "ProcessRule.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

// ── 保存为 JSON ───────────────────────────────
bool ProcessRule::saveToFile(const QString& path, QString* err) const {
    QJsonArray fArr;
    for (const auto& f : formulas) {
        QJsonObject fObj;
        fObj["name"] = f.name;
        fObj["type"] = f.type;
        fObj["colA"] = f.colA;
        fObj["colB"] = f.colB;

        if (f.type == "classify") {
            QJsonArray cArr;
            for (const auto& c : f.conditions) {
                QJsonObject cObj;
                cObj["threshold"] = c.threshold;
                cObj["op"]        = c.op;
                cObj["result"]    = c.result.toString();
                cArr.append(cObj);
            }
            fObj["conditions"] = cArr;
        }
        fArr.append(fObj);
    }

    QJsonObject statObj;
    QJsonArray stCols;
    for (const auto& c : stat.statCols) stCols.append(c);
    statObj["statCols"] = stCols;
    statObj["groupCol"] = stat.groupCol;

    QJsonObject root;
    root["formulas"] = fArr;
    root["stat"]     = statObj;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        if (err) *err = "无法写入: " + path;
        return false;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    qDebug() << "[ProcessRule] 保存规则:" << path;
    return true;
}

// ── 从 JSON 加载 ──────────────────────────────
ProcessRule ProcessRule::loadFromFile(const QString& path, QString* err) {
    ProcessRule rule;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (err) *err = "无法读取: " + path;
        return rule;
    }

    QJsonParseError pe;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &pe);
    if (pe.error != QJsonParseError::NoError) {
        if (err) *err = "JSON解析错误: " + pe.errorString();
        return rule;
    }

    QJsonObject root = doc.object();

    for (const auto& fv : root["formulas"].toArray()) {
        QJsonObject fo = fv.toObject();
        FormulaRule fr;
        fr.name = fo["name"].toString();
        fr.type = fo["type"].toString();
        fr.colA = fo["colA"].toString();
        fr.colB = fo["colB"].toString();

        if (fr.type == "classify") {
            for (const auto& cv : fo["conditions"].toArray()) {
                QJsonObject co = cv.toObject();
                ConditionRule cr;
                cr.threshold = co["threshold"].toDouble();
                cr.op        = co["op"].toString();
                cr.result    = co["result"].toString();
                fr.conditions << cr;
            }
        }
        rule.formulas << fr;
    }

    QJsonObject so = root["stat"].toObject();
    for (const auto& v : so["statCols"].toArray())
        rule.stat.statCols << v.toString();
    rule.stat.groupCol = so["groupCol"].toString();

    qDebug() << "[ProcessRule] 加载规则:" << path
             << "公式数:" << rule.formulas.size();
    return rule;
}