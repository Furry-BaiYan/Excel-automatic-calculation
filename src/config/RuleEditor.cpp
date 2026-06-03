#include "RuleEditor.h"
#include <QTabWidget>
#include <QTableWidget>
#include <QListWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QDebug>

RuleEditor::RuleEditor(const QStringList& columns, QWidget* parent)
    : QDialog(parent), m_cols(columns) {
    setWindowTitle("规则配置器");
    setMinimumSize(780, 580);
    buildUI();
    refreshColCombos();
}

void RuleEditor::buildUI() {
    auto* mainLayout = new QVBoxLayout(this);

    // 顶部说明
    auto* tip = new QLabel("程序会自动探测列名，配置好规则后点确定运行。", this);
    tip->setStyleSheet("color:#555; padding:4px;");
    mainLayout->addWidget(tip);

    auto* tabs = new QTabWidget(this);

    auto* tab1 = new QWidget; buildFormulaTab(tab1);
    auto* tab2 = new QWidget; buildClassifyTab(tab2);
    auto* tab3 = new QWidget; buildStatTab(tab3);

    tabs->addTab(tab1, "➕ 算术公式");
    tabs->addTab(tab2, "🏷 条件分级");
    tabs->addTab(tab3, "📊 统计配置");
    mainLayout->addWidget(tabs);

    // 底部：保存/加载 + 确定/取消
    auto* btnRow = new QHBoxLayout;
    auto* btnSave = new QPushButton("💾 保存规则", this);
    auto* btnLoad = new QPushButton("📂 加载规则", this);
    connect(btnSave, &QPushButton::clicked, this, &RuleEditor::onSaveRule);
    connect(btnLoad, &QPushButton::clicked, this, &RuleEditor::onLoadRule);
    btnRow->addWidget(btnSave);
    btnRow->addWidget(btnLoad);
    btnRow->addStretch();

    auto* box = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    btnRow->addWidget(box);
    mainLayout->addLayout(btnRow);
}

// ── Tab1：算术公式 ─────────────────────────────
void RuleEditor::buildFormulaTab(QWidget* tab) {
    auto* lay = new QVBoxLayout(tab);

    m_arithTable = new QTableWidget(0, 4, tab);
    m_arithTable->setHorizontalHeaderLabels({"新列名", "运算", "列 A", "列 B"});
    m_arithTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_arithTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_arithTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    lay->addWidget(m_arithTable);

    auto* btnRow = new QHBoxLayout;
    auto* btnAdd = new QPushButton("➕ 添加公式", tab);
    auto* btnDel = new QPushButton("🗑 删除选中", tab);
    connect(btnAdd, &QPushButton::clicked, this, &RuleEditor::onAddArith);
    connect(btnDel, &QPushButton::clicked, this, &RuleEditor::onRemoveFormula);
    btnRow->addWidget(btnAdd);
    btnRow->addWidget(btnDel);
    btnRow->addStretch();

    // 内联输入区
    auto* grp = new QGroupBox("添加一条算术公式", tab);
    auto* fl  = new QFormLayout(grp);

    auto* nameEdit = new QLineEdit(grp); nameEdit->setObjectName("arithName");
    auto* opBox    = new QComboBox(grp);  opBox->setObjectName("arithOp");
    auto* colABox  = new QComboBox(grp);  colABox->setObjectName("arithColA");
    auto* colBBox  = new QComboBox(grp);  colBBox->setObjectName("arithColB");
    colBBox->setEditable(true);                                              
    colBBox->lineEdit()->setPlaceholderText("选列名或直接输数字，如：3");    

    opBox->addItems({"除法 (A÷B)", "减法 (A−B)", "加法 (A+B)", "乘法 (A×B)"});
    

    opBox->addItems({"除法 (A÷B)", "减法 (A−B)", "加法 (A+B)", "乘法 (A×B)"});

    fl->addRow("新列名：",  nameEdit);
    fl->addRow("运算类型：", opBox);
    fl->addRow("列 A：",    colABox);
    fl->addRow("列 B：",    colBBox);

    btnRow->addWidget(grp);
    lay->addLayout(btnRow);
}

// ── Tab2：条件分级 ─────────────────────────────
void RuleEditor::buildClassifyTab(QWidget* tab) {
    auto* lay = new QVBoxLayout(tab);

    auto* topRow = new QHBoxLayout;
    auto* grp    = new QGroupBox("分级设置", tab);
    auto* fl     = new QFormLayout(grp);

    m_clsNewName = new QLineEdit(grp);
    m_clsSrcCol  = new QComboBox(grp);
    fl->addRow("新列名：",  m_clsNewName);
    fl->addRow("来源列：",  m_clsSrcCol);
    topRow->addWidget(grp);
    topRow->addStretch();
    lay->addLayout(topRow);

    // 条件表：阈值 | 运算符 | 标签
    auto* condLabel = new QLabel("条件列表（从上往下匹配，第一个满足的生效）：", tab);
    lay->addWidget(condLabel);

    m_clsTable = new QTableWidget(0, 3, tab);
    m_clsTable->setHorizontalHeaderLabels({"阈值（数字）", "运算符", "标签（文本）"});
    m_clsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    lay->addWidget(m_clsTable);

    auto* btnRow = new QHBoxLayout;
    auto* btnAdd = new QPushButton("➕ 添加条件", tab);
    auto* btnDel = new QPushButton("🗑 删除条件", tab);
    auto* btnConfirm = new QPushButton("✅ 确认添加分级规则", tab);
    btnConfirm->setStyleSheet("background:#4472C4; color:white; font-weight:bold;");

    connect(btnAdd, &QPushButton::clicked, this, &RuleEditor::onAddCondition);
    connect(btnDel, &QPushButton::clicked, this, &RuleEditor::onRemoveCondition);
    connect(btnConfirm, &QPushButton::clicked, this, &RuleEditor::onAddClassify);

    btnRow->addWidget(btnAdd);
    btnRow->addWidget(btnDel);
    btnRow->addStretch();
    btnRow->addWidget(btnConfirm);
    lay->addLayout(btnRow);

    // 提示
    auto* eg = new QLabel(
        "示例：来源列=销售额，条件：≥8000→优秀  ≥5000→良好  ≥0→待改进", tab);
    eg->setStyleSheet("color:#888; font-size:11px;");
    lay->addWidget(eg);
}

// ── Tab3：统计配置 ─────────────────────────────
void RuleEditor::buildStatTab(QWidget* tab) {
    auto* lay = new QVBoxLayout(tab);

    lay->addWidget(new QLabel("选择要统计的数值列（合计/均值/最大/最小）：", tab));
    m_statColList = new QListWidget(tab);
    for (const auto& col : m_cols) {
        auto* item = new QListWidgetItem(col, m_statColList);
        item->setCheckState(Qt::Unchecked);
    }
    lay->addWidget(m_statColList);

    auto* fl = new QFormLayout;
    m_groupColBox = new QComboBox(tab);
    m_groupColBox->addItem("（不分组）", "");
    for (const auto& col : m_cols)
        m_groupColBox->addItem(col, col);
    fl->addRow("分组列：", m_groupColBox);
    lay->addLayout(fl);
}

// ── 刷新所有列名下拉框 ─────────────────────────
void RuleEditor::refreshColCombos() {
    auto fillCombo = [&](const QString& objName) {
        auto* cb = findChild<QComboBox*>(objName);
        if (!cb) return;
        cb->clear();
        for (const auto& c : m_cols) cb->addItem(c);
    };
    fillCombo("arithColA");
    fillCombo("arithColB");
    if (m_clsSrcCol) {
        m_clsSrcCol->clear();
        for (const auto& c : m_cols) m_clsSrcCol->addItem(c);
    }
}

// ── 添加一条算术公式到表格 ─────────────────────
void RuleEditor::onAddArith() {
    auto* nameEdit = findChild<QLineEdit*>("arithName");
    auto* opBox    = findChild<QComboBox*>("arithOp");
    auto* colABox  = findChild<QComboBox*>("arithColA");
    auto* colBBox  = findChild<QComboBox*>("arithColB");

    if (!nameEdit || nameEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "提示", "请填写新列名");
        return;
    }

    QString newName = nameEdit->text().trimmed(); // ← 先保存，再 clear

    int row = m_arithTable->rowCount();
    m_arithTable->insertRow(row);
    m_arithTable->setItem(row, 0, new QTableWidgetItem(newName));
    m_arithTable->setItem(row, 1, new QTableWidgetItem(opBox->currentText()));
    m_arithTable->setItem(row, 2, new QTableWidgetItem(colABox->currentText()));
    m_arithTable->setItem(row, 3, new QTableWidgetItem(colBBox->currentText()));

    nameEdit->clear();

    // ── 把新列名同步到所有下拉框 ──────────────────
    if (colABox->findText(newName) == -1) colABox->addItem(newName);
    if (colBBox->findText(newName) == -1) colBBox->addItem(newName);
    if (m_clsSrcCol && m_clsSrcCol->findText(newName) == -1)
        m_clsSrcCol->addItem(newName);

    // 统计列列表也加入
    bool exists = false;
    for (int i = 0; i < m_statColList->count(); ++i)
        if (m_statColList->item(i)->text() == newName) { exists = true; break; }
    if (!exists) {
        auto* item = new QListWidgetItem(newName, m_statColList);
        item->setCheckState(Qt::Unchecked);
    }
}

// ── 添加分级规则 ───────────────────────────────
void RuleEditor::onAddClassify() {
    if (m_clsNewName->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "提示", "请填写新列名");
        return;
    }
    if (m_clsTable->rowCount() == 0) {
        QMessageBox::warning(this, "提示", "请至少添加一条条件");
        return;
    }
    // 将分级规则也加入 arithTable 作展示（type=classify，colA=来源列）
    int row = m_arithTable->rowCount();
    m_arithTable->insertRow(row);
    m_arithTable->setItem(row, 0, new QTableWidgetItem(m_clsNewName->text().trimmed()));
    m_arithTable->setItem(row, 1, new QTableWidgetItem("条件分级"));
    m_arithTable->setItem(row, 2, new QTableWidgetItem(m_clsSrcCol->currentText()));
    // 把条件序列化为摘要文本放在列B
    QStringList summary;
    for (int r = 0; r < m_clsTable->rowCount(); ++r) {
    auto* spin  = qobject_cast<QDoubleSpinBox*>(m_clsTable->cellWidget(r, 0));
    auto* opBox = qobject_cast<QComboBox*>(m_clsTable->cellWidget(r, 1));
    auto* label = m_clsTable->item(r, 2);
    if (spin && opBox && label && !label->text().trimmed().isEmpty())
        summary << QString("%1%2→%3")
                   .arg(opBox->currentText())
                   .arg(spin->value(), 0, 'f', 1)
                   .arg(label->text().trimmed());
}
    m_arithTable->setItem(row, 3, new QTableWidgetItem(summary.join("; ")));
    m_clsNewName->clear();
    m_clsTable->setRowCount(0);
}

// ── 删除选中公式 ───────────────────────────────
void RuleEditor::onRemoveFormula() {
    auto rows = m_arithTable->selectedItems();
    if (rows.isEmpty()) return;
    int row = m_arithTable->currentRow();
    m_arithTable->removeRow(row);
}

// ── 添加/删除条件行 ────────────────────────────
void RuleEditor::onAddCondition() {
    int row = m_clsTable->rowCount();
    m_clsTable->insertRow(row);

    auto* spinBox = new QDoubleSpinBox;
    spinBox->setRange(-1e9, 1e9);
    spinBox->setDecimals(1);
    m_clsTable->setCellWidget(row, 0, spinBox);

    auto* opBox = new QComboBox;
    opBox->addItems({">=", ">", "<=", "<", "=="});
    m_clsTable->setCellWidget(row, 1, opBox);

    m_clsTable->setItem(row, 2, new QTableWidgetItem(""));
}

void RuleEditor::onRemoveCondition() {
    int row = m_clsTable->currentRow();
    if (row >= 0) m_clsTable->removeRow(row);
}

// ── 保存规则到 JSON ────────────────────────────
void RuleEditor::onSaveRule() {
    QString path = QFileDialog::getSaveFileName(
        this, "保存规则", "", "规则文件 (*.json)");
    if (path.isEmpty()) return;
    ProcessRule rule = getRule();
    QString err;
    if (rule.saveToFile(path, &err))
        QMessageBox::information(this, "保存成功", "规则已保存：\n" + path);
    else
        QMessageBox::warning(this, "保存失败", err);
}

// ── 从 JSON 加载规则 ───────────────────────────
void RuleEditor::onLoadRule() {
    QString path = QFileDialog::getOpenFileName(
        this, "加载规则", "", "规则文件 (*.json)");
    if (path.isEmpty()) return;
    QString err;
    ProcessRule rule = ProcessRule::loadFromFile(path, &err);
    if (!err.isEmpty()) { QMessageBox::warning(this, "加载失败", err); return; }
    setRule(rule);
    QMessageBox::information(this, "加载成功",
        QString("已加载 %1 条公式规则").arg(rule.formulas.size()));
}

// ── 预加载规则 ─────────────────────────────────
void RuleEditor::setRule(const ProcessRule& rule) {
    m_arithTable->setRowCount(0);

    for (const auto& f : rule.formulas) {
        int row = m_arithTable->rowCount();
        m_arithTable->insertRow(row);
        m_arithTable->setItem(row, 0, new QTableWidgetItem(f.name));

        QString typeStr;
        if      (f.type == "divide")   typeStr = "除法 (A÷B)";
        else if (f.type == "subtract") typeStr = "减法 (A−B)";
        else if (f.type == "add")      typeStr = "加法 (A+B)";
        else if (f.type == "multiply") typeStr = "乘法 (A×B)";
        else if (f.type == "classify") typeStr = "条件分级";
        m_arithTable->setItem(row, 1, new QTableWidgetItem(typeStr));
        m_arithTable->setItem(row, 2, new QTableWidgetItem(f.colA));

        if (f.type == "classify") {
            QStringList s;
            for (const auto& c : f.conditions)
                s << QString("%1%2→%3").arg(c.op).arg(c.threshold).arg(c.result.toString());
            m_arithTable->setItem(row, 3, new QTableWidgetItem(s.join("; ")));
        } else {
            m_arithTable->setItem(row, 3, new QTableWidgetItem(f.colB));
        }
    }

    // 统计配置
    for (int i = 0; i < m_statColList->count(); ++i) {
        auto* item = m_statColList->item(i);
        item->setCheckState(
            rule.stat.statCols.contains(item->text())
            ? Qt::Checked : Qt::Unchecked);
    }
    int gi = m_groupColBox->findData(rule.stat.groupCol);
    if (gi >= 0) m_groupColBox->setCurrentIndex(gi);
}

// ── 读取规则 ───────────────────────────────────
ProcessRule RuleEditor::getRule() const {
    ProcessRule rule;

    static const QMap<QString,QString> typeMap = {
        {"除法 (A÷B)", "divide"},
        {"减法 (A−B)", "subtract"},
        {"加法 (A+B)", "add"},
        {"乘法 (A×B)", "multiply"},
        {"条件分级",   "classify"}
    };

    for (int r = 0; r < m_arithTable->rowCount(); ++r) {
        FormulaRule f;
        f.name = m_arithTable->item(r,0) ? m_arithTable->item(r,0)->text() : "";
        QString typeStr = m_arithTable->item(r,1) ? m_arithTable->item(r,1)->text() : "";
        f.type = typeMap.value(typeStr, "add");
        f.colA = m_arithTable->item(r,2) ? m_arithTable->item(r,2)->text() : "";

        if (f.type == "classify") {
            // 解析摘要文本：">=8000→优秀; >=5000→良好"
            QString summary = m_arithTable->item(r,3)
                              ? m_arithTable->item(r,3)->text() : "";
            for (const auto& part : summary.split("; ", Qt::SkipEmptyParts)) {
                ConditionRule cr;
                // 找运算符
                for (const auto& op : {">=","<=",">","<","=="}) {
                    if (part.startsWith(op)) {
                        cr.op = op;
                        QString rest = part.mid(strlen(op));
                        int arrow = rest.indexOf("→");
                        if (arrow >= 0) {
                            cr.threshold = rest.left(arrow).toDouble();
                            cr.result    = rest.mid(arrow + 1);
                        }
                        break;
                    }
                }
                if (!cr.op.isEmpty()) f.conditions << cr;
            }
        } else {
            f.colB = m_arithTable->item(r,3) ? m_arithTable->item(r,3)->text() : "";
        }
        if (!f.name.isEmpty()) rule.formulas << f;
    }

    // 统计配置
    for (int i = 0; i < m_statColList->count(); ++i) {
        auto* item = m_statColList->item(i);
        if (item->checkState() == Qt::Checked)
            rule.stat.statCols << item->text();
    }
    rule.stat.groupCol = m_groupColBox->currentData().toString();

    return rule;
}

void RuleEditor::onFormulaTypeChanged(int) {}