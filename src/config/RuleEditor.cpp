#include "RuleEditor.h"
#include "FormulaParser.h"
#include "AIAssistant.h"
#include "ApiSettingsDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QLineEdit>
#include <QListWidget>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QFont>

RuleEditor::RuleEditor(const QStringList& columns, QWidget* parent)
    : QDialog(parent), m_cols(columns) {
    setWindowTitle("规则配置器");
    setMinimumSize(720, 640);

    m_ai = new AIAssistant(this);
    connect(m_ai, &AIAssistant::suggestionsReady, this, &RuleEditor::onAISuggestions);
    connect(m_ai, &AIAssistant::errorOccurred,    this, &RuleEditor::onAIError);
    connect(m_ai, &AIAssistant::analyzing,         this, &RuleEditor::onAIAnalyzing);

    auto* lay = new QVBoxLayout(this);

    // ── 顶部：列名 + AI 按钮 ──────────────────────
    auto* topRow = new QHBoxLayout;
    auto* colLabel = new QLabel(
        "📋 列名：<b>" + m_cols.join("　") + "</b>", this);
    colLabel->setWordWrap(true);
    colLabel->setStyleSheet(
        "background:#1e3a5f;color:#aad4ff;padding:8px;border-radius:4px;");
    topRow->addWidget(colLabel, 1);

    m_btnAI = new QPushButton("🤖 AI 分析推荐", this);
    m_btnAI->setStyleSheet(
        "background:#6b2fb3;color:white;font-weight:bold;"
        "padding:8px 16px;border-radius:4px;min-width:120px;");
    auto* btnKey = new QPushButton("⚙️", this);
    btnKey->setFixedWidth(32);
    btnKey->setToolTip("设置 API");

    connect(m_btnAI, &QPushButton::clicked, this, &RuleEditor::onAIAnalyze);
    connect(btnKey,  &QPushButton::clicked, this, &RuleEditor::onSetApiKey);
    topRow->addWidget(m_btnAI);
    topRow->addWidget(btnKey);
    lay->addLayout(topRow);

    // ── 报告标题 ──────────────────────────────────
    auto* titleRow = new QHBoxLayout;
    titleRow->addWidget(new QLabel("报告标题：", this));
    m_titleEdit = new QLineEdit(this);
    m_titleEdit->setPlaceholderText("AI 自动生成，也可手动修改");
    titleRow->addWidget(m_titleEdit, 1);
    lay->addLayout(titleRow);

    // ── 格式要求 ──────────────────────────────────
    auto* fmtGrp = new QGroupBox("格式要求（可选，留空则 AI 自动判断）", this);
    auto* fmtLay = new QVBoxLayout(fmtGrp);
    m_formatReq = new QTextEdit(fmtGrp);
    m_formatReq->setMaximumHeight(60);
    m_formatReq->setFont(QFont("Microsoft YaHei", 11));
    m_formatReq->setPlaceholderText(
        "示例：等级分为 优秀/良好/及格/不及格，金额保留2位小数，工资超过10000为高薪");
    fmtLay->addWidget(m_formatReq);
    lay->addWidget(fmtGrp);

    // ── 语法提示 ──────────────────────────────────
    auto* tip = new QLabel(
        "✏️ 每行一条，格式：<b>新列名 = 表达式</b>　　"
        "支持：<code>+ - * / ( )  IF  IFS  SUM  AVERAGE  ROUND  MAX  MIN ...</code>",
        this);
    tip->setWordWrap(true);
    tip->setStyleSheet("color:#888;font-size:11px;padding:2px;");
    lay->addWidget(tip);

    // ── 公式编辑器 ────────────────────────────────
    m_editor = new QTextEdit(this);
    m_editor->setFont(QFont("Consolas", 13));
    m_editor->setMinimumHeight(200);
    m_editor->setPlaceholderText(
        "# 点击【🤖 AI 分析推荐】自动生成公式\n"
        "# 或手动输入，示例：\n\n"
        "总分 = 数学 + 语文 + 英语\n"
        "平均分 = ROUND(总分 / 3, 1)\n"
        "等级 = IFS(总分>=270, \"优秀\", 总分>=225, \"良好\", \"待改进\")");
    lay->addWidget(m_editor);

    // ── 状态栏 ────────────────────────────────────
    m_status = new QLabel("", this);
    m_status->setWordWrap(true);
    m_status->setMinimumHeight(24);
    lay->addWidget(m_status);

    // ── 统计配置 ──────────────────────────────────
    auto* statGrp = new QGroupBox("统计配置（可选，AI 自动选择）", this);
    auto* sg = new QHBoxLayout(statGrp);

    auto* leftLay = new QVBoxLayout;
    leftLay->addWidget(new QLabel("统计列（勾选要汇总的列）："));
    m_statList = new QListWidget;
    m_statList->setMaximumHeight(90);
    for (const auto& c : m_cols) {
        auto* item = new QListWidgetItem(c, m_statList);
        item->setCheckState(Qt::Unchecked);
    }
    leftLay->addWidget(m_statList);

    auto* rightLay = new QFormLayout;
    m_groupBox = new QComboBox;
    m_groupBox->addItem("（不分组）", "");
    for (const auto& c : m_cols) m_groupBox->addItem(c, c);
    rightLay->addRow("分组列：", m_groupBox);

    sg->addLayout(leftLay, 2);
    sg->addLayout(rightLay, 1);
    lay->addWidget(statGrp);

    // ── 底部按钮 ──────────────────────────────────
    auto* bRow = new QHBoxLayout;
    auto* bVal  = new QPushButton("✅ 验证公式", this);
    auto* bSave = new QPushButton("💾 保存规则", this);
    auto* bLoad = new QPushButton("📂 加载规则", this);
    bVal->setStyleSheet("background:#217346;color:white;font-weight:bold;");

    connect(bVal,  &QPushButton::clicked, this, &RuleEditor::onValidate);
    connect(bSave, &QPushButton::clicked, this, &RuleEditor::onSave);
    connect(bLoad, &QPushButton::clicked, this, &RuleEditor::onLoad);

    auto* box = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(box, &QDialogButtonBox::accepted, this, [this]() {
        onValidate();
        if (!m_status->text().startsWith("❌")) accept();
    });
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);

    bRow->addWidget(bVal);
    m_btnFix = new QPushButton("🔧 AI 修复", this);
    m_btnFix->setStyleSheet("background:#8b4513;color:white;font-weight:bold;");
    m_btnFix->setVisible(false); // 默认隐藏
    connect(m_btnFix, &QPushButton::clicked, this, &RuleEditor::onAIFix);
    bRow->addWidget(m_btnFix);
    bRow->addWidget(bSave);
    bRow->addWidget(bLoad);
    bRow->addStretch();
    bRow->addWidget(box);
    lay->addLayout(bRow);
}

void RuleEditor::setSampleData(const QVector<QStringList>& rows) {
    m_sampleRows = rows;
}

// ── AI 分析 ────────────────────────────────────
void RuleEditor::onAIAnalyze() {
    if (m_ai->config().apiKey.trimmed().isEmpty()) {
        onSetApiKey();
        if (m_ai->config().apiKey.trimmed().isEmpty()) return;
    }
    m_ai->analyze(m_cols, m_sampleRows,
                  m_formatReq ? m_formatReq->toPlainText().trimmed() : "");
}

void RuleEditor::onAIAnalyzing() {
    m_btnAI->setEnabled(false);
    m_btnAI->setText("⏳ AI 分析中...");
    m_status->setText("🤖 正在分析数据，请稍候...");
    m_status->setStyleSheet("color:#6b2fb3;font-weight:bold;");
}

void RuleEditor::onAISuggestions(const QString& text) {
    m_btnAI->setEnabled(true);
    m_btnAI->setText("🤖 AI 分析推荐");

    // ── 解析公式、统计配置、报告标题 ─────────────
    QStringList formulaLines;
    QStringList statCols;
    QString     groupCol;
    QString     aiTitle;
    bool inStatSection  = false;
    bool inTitleSection = false;

    for (const auto& line : text.split('\n')) {
        QString t = line.trimmed();

        // 检测段落
        if (t.startsWith("## 报告标题") || t == "# 报告标题") {
            inTitleSection = true; inStatSection = false; continue;
        }
        if (t.startsWith("## 统计配置") || t == "# 统计配置" ||
            t.contains("统计配置")) {
            inStatSection = true; inTitleSection = false; continue;
        }
        // 新段落重置
        if (t.startsWith("##") || t.startsWith("# ")) {
            inStatSection = false; inTitleSection = false;
        }

        if (inTitleSection) {
            if (!t.isEmpty() && !t.startsWith("#") && !t.startsWith("##")) {
                aiTitle = t; inTitleSection = false;
            }
            continue;
        }

        if (inStatSection) {
            // 统计列
            if (t.startsWith("统计列")) {
                int colon = t.indexOf(QChar(':'));
                if (colon < 0) colon = t.indexOf(QString("："));
                if (colon >= 0) {
                    for (const auto& c : t.mid(colon+1).split(','))
                        if (!c.trimmed().isEmpty() && c.trimmed() != "无")
                            statCols << c.trimmed();
                }
            }
            // 分组列
            else if (t.startsWith("分组列")) {
                int colon = t.indexOf(QChar(':'));
                if (colon < 0) colon = t.indexOf(QString("："));
                if (colon >= 0) {
                    groupCol = t.mid(colon+1).trimmed();
                    if (groupCol == "无" || groupCol.toLower() == "none")
                        groupCol = "";
                }
            }
            continue;
        }

        formulaLines << line;
    }

    // ── 填入报告标题 ──────────────────────────────
    if (!aiTitle.isEmpty() && m_titleEdit->text().trimmed().isEmpty())
        m_titleEdit->setText(aiTitle);

    // ── 填入公式编辑器 ────────────────────────────
    QString formulas = formulaLines.join('\n').trimmed();
    QString current  = m_editor->toPlainText().trimmed();
    if (current.isEmpty())
        m_editor->setPlainText(formulas);
    else
        m_editor->setPlainText(current + "\n\n# ── AI 新建议 ──\n" + formulas);

    // ── 自动配置统计 ──────────────────────────────
    onValidate(); // 先把新公式列加入列表
    if (m_btnFix) m_btnFix->setVisible(false); // ← 加这行：AI推荐成功，隐藏修复按钮
    if (!statCols.isEmpty() || !groupCol.isEmpty())
        setStatConfig(groupCol, statCols);

    QString titleInfo = aiTitle.isEmpty() ? "" : QString("  标题：%1").arg(aiTitle);
    m_status->setText(
        QString("✅ AI 完成：统计列 %1 个，分组：%2%3")
        .arg(statCols.size())
        .arg(groupCol.isEmpty() ? "无" : groupCol)
        .arg(titleInfo));
    m_status->setStyleSheet("color:#00cc66;font-weight:bold;");
}

void RuleEditor::onAIError(const QString& msg) {
    m_btnAI->setEnabled(true);
    m_btnAI->setText("🤖 AI 分析推荐");
    m_status->setText("❌ " + msg);
    m_status->setStyleSheet("color:#ff4444;");
}

void RuleEditor::onSetApiKey() {
    ApiSettingsDialog dlg(m_ai->config(), this);
    if (dlg.exec() == QDialog::Accepted)
        m_ai->setConfig(dlg.result());
}

// ── 验证公式 ───────────────────────────────────
void RuleEditor::onValidate() {
    auto r = FormulaParser::parse(m_editor->toPlainText(), m_cols);

    QStringList existing;
    for (int i=0;i<m_statList->count();++i)
        existing << m_statList->item(i)->text();

    for (const auto& f : r.formulas) {
        if (!existing.contains(f.name)) {
            auto* item = new QListWidgetItem(f.name, m_statList);
            item->setCheckState(Qt::Unchecked);
            existing << f.name;
        }
        if (m_groupBox->findData(f.name)==-1)
            m_groupBox->addItem(f.name, f.name);
    }

    if (r.ok()) {
        m_status->setText(QString("✅ %1 条公式解析成功").arg(r.formulas.size()));
        m_status->setStyleSheet("color:#00cc66;font-weight:bold;");
    } else {
        m_status->setText("❌ " + r.errors.join("\n"));
        m_status->setStyleSheet("color:#ff4444;");
    }
}

// ── 保存/加载规则 ──────────────────────────────
void RuleEditor::onSave() {
    QString path = QFileDialog::getSaveFileName(
        this, "保存规则", "", "规则文件 (*.json)");
    if (path.isEmpty()) return;
    QJsonObject obj;
    obj["formulas"]    = m_editor->toPlainText();
    obj["reportTitle"] = m_titleEdit->text();
    obj["formatReq"]   = m_formatReq ? m_formatReq->toPlainText() : "";
    auto [gc, sc] = getStatConfig();
    QJsonArray cols; for (const auto& c : sc) cols.append(c);
    obj["statCols"] = cols; obj["groupCol"] = gc;
    QFile f(path);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(obj).toJson());
        QMessageBox::information(this, "✅ 保存成功", path);
    }
}

void RuleEditor::onLoad() {
    QString path = QFileDialog::getOpenFileName(
        this, "加载规则", "", "规则文件 (*.json)");
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return;
    auto obj = QJsonDocument::fromJson(f.readAll()).object();
    m_editor->setPlainText(obj["formulas"].toString());
    m_titleEdit->setText(obj["reportTitle"].toString());
    if (m_formatReq) m_formatReq->setPlainText(obj["formatReq"].toString());
    QStringList sc;
    for (const auto& v : obj["statCols"].toArray()) sc << v.toString();
    setStatConfig(obj["groupCol"].toString(), sc);
    QMessageBox::information(this, "✅ 加载成功", "规则已加载");
}

// ── Getter / Setter ────────────────────────────
void RuleEditor::setFormulaText(const QString& t) { m_editor->setPlainText(t); }
QString RuleEditor::getFormulaText() const { return m_editor->toPlainText(); }

QString RuleEditor::getReportTitle() const {
    return m_titleEdit ? m_titleEdit->text().trimmed() : "";
}
void RuleEditor::setReportTitle(const QString& t) {
    if (m_titleEdit) m_titleEdit->setText(t);
}

void RuleEditor::setStatConfig(const QString& gc, const QStringList& sc) {
    for (int i=0;i<m_statList->count();++i) {
        auto* item=m_statList->item(i);
        item->setCheckState(sc.contains(item->text())?Qt::Checked:Qt::Unchecked);
    }
    int gi=m_groupBox->findData(gc);
    if (gi>=0) m_groupBox->setCurrentIndex(gi);
}

void RuleEditor::onAIFix() {
    if (m_ai->config().apiKey.trimmed().isEmpty()) {
        onSetApiKey();
        if (m_ai->config().apiKey.trimmed().isEmpty()) return;
    }

    // 收集当前错误
    auto r = FormulaParser::parse(m_editor->toPlainText(), m_cols);
    if (r.ok()) { m_btnFix->setVisible(false); return; }

    // 连接修复结果到编辑器（用 lambda 只响应一次）
    QMetaObject::Connection* conn = new QMetaObject::Connection;
    *conn = connect(m_ai, &AIAssistant::suggestionsReady,
                    this, [this, conn](const QString& fixed) {
        disconnect(*conn); delete conn;
        m_editor->setPlainText(fixed);
        m_status->setText("🔧 AI 已修复，请验证");
        m_status->setStyleSheet("color:#ffaa00;font-weight:bold;");
        m_btnFix->setVisible(false);
        onValidate();
    });

    m_ai->fixFormulas(m_editor->toPlainText(), r.errors, m_cols);
}

QPair<QString,QStringList> RuleEditor::getStatConfig() const {
    QStringList sc;
    for (int i=0;i<m_statList->count();++i) {
        auto* item=m_statList->item(i);
        if (item->checkState()==Qt::Checked) sc<<item->text();
    }
    return {m_groupBox->currentData().toString(), sc};
}