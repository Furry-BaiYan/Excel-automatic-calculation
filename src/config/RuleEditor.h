#pragma once
#include <QDialog>
#include <QStringList>
#include "ProcessRule.h"

class QTabWidget;
class QTableWidget;
class QListWidget;
class QComboBox;
class QLineEdit;
class QPushButton;

class RuleEditor : public QDialog {
    Q_OBJECT
public:
    // columns: 当前文件探测到的列名列表
    explicit RuleEditor(const QStringList& columns,
                        QWidget* parent = nullptr);

    // 预加载已有规则（可选）
    void setRule(const ProcessRule& rule);

    // 获取用户配置的规则
    ProcessRule getRule() const;

private slots:
    void onAddArith();       // 添加算术公式
    void onAddClassify();    // 添加分级规则
    void onRemoveFormula();  // 删除选中公式
    void onAddCondition();   // 添加分级条件行
    void onRemoveCondition();
    void onSaveRule();
    void onLoadRule();
    void onFormulaTypeChanged(int idx);

private:
    void buildUI();
    void buildFormulaTab(QWidget* tab);
    void buildClassifyTab(QWidget* tab);
    void buildStatTab(QWidget* tab);
    void refreshColCombos(); // 刷新所有列名下拉框

    QStringList   m_cols;

    // 算术公式 tab
    QTableWidget* m_arithTable  = nullptr;

    // 分级规则 tab
    QComboBox*    m_clsSrcCol   = nullptr;
    QLineEdit*    m_clsNewName  = nullptr;
    QTableWidget* m_clsTable    = nullptr;  // threshold | op | label

    // 统计配置 tab
    QListWidget*  m_statColList = nullptr;
    QComboBox*    m_groupColBox = nullptr;
};