#include <QApplication>
#include <QMainWindow>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QFileDialog>
#include <QMessageBox>
#include <QLabel>
#include "io/XlsxReader.h"
#include "io/XlsxWriter.h"
#include "io/PdfExporter.h"
#include "core/FormulaEngine.h"
#include "core/StatEngine.h"
#include "config/ProcessRule.h"
#include "config/RuleEditor.h"

class MainWindow : public QMainWindow {
public:
    MainWindow(QWidget* parent = nullptr) : QMainWindow(parent) {
        setWindowTitle("Excel Processor");
        setMinimumSize(960, 700);

        auto* central    = new QWidget(this);
        auto* mainLayout = new QVBoxLayout(central);

        // 状态栏
        statusLabel = new QLabel("请打开一个 xlsx 文件", this);
        statusLabel->setStyleSheet(
            "background:#1e3a5f; color:white; padding:6px 12px; font-size:13px;");

        // 按钮行
        auto* btnRow = new QHBoxLayout;
        btnOpen      = new QPushButton("📂 打开 xlsx",    this);
        btnConfig    = new QPushButton("⚙️  配置规则",     this);
        btnRun       = new QPushButton("▶  执行计算",      this);
        btnExportXlsx= new QPushButton("💾 导出 xlsx",    this);
        btnExportPdf = new QPushButton("📄 导出 PDF",     this);

        btnConfig->setEnabled(false);
        btnRun->setEnabled(false);
        btnExportXlsx->setEnabled(false);
        btnExportPdf ->setEnabled(false);

        btnRun->setStyleSheet(
            "background:#217346; color:white; font-weight:bold; padding:6px 18px;");

        for (auto* b : {btnOpen, btnConfig, btnRun, btnExportXlsx, btnExportPdf})
            btnRow->addWidget(b);

        output = new QTextEdit(this);
        output->setReadOnly(true);
        output->setFont(QFont("Consolas", 10));

        mainLayout->addWidget(statusLabel);
        mainLayout->addLayout(btnRow);
        mainLayout->addWidget(output);
        setCentralWidget(central);

        // ── 打开文件 ──────────────────────────────
        connect(btnOpen, &QPushButton::clicked, [this]() {
            QString path = QFileDialog::getOpenFileName(
                this, "选择Excel文件", "", "Excel Files (*.xlsx)");
            if (path.isEmpty()) return;

            QString errMsg;
            rawTable = XlsxReader::readFile(path, {}, &errMsg);
            if (!errMsg.isEmpty()) {
                statusLabel->setText("❌ 读取失败：" + errMsg);
                return;
            }

            statusLabel->setText(QString("✅ 已读取：%1  |  %2行 × %3列  |  "
                "列：%4")
                .arg(rawTable.name)
                .arg(rawTable.rowCount())
                .arg(rawTable.columnCount())
                .arg(rawTable.columnNames().join(", ")));

            output->setText("文件已读取，请点击【⚙️ 配置规则】设置计算逻辑。\n\n"
                            "探测到的列：\n" + rawTable.columnNames().join("\n"));

            btnConfig->setEnabled(true);
            btnRun->setEnabled(false);
            btnExportXlsx->setEnabled(false);
            btnExportPdf ->setEnabled(false);
        });

        // ── 配置规则 ──────────────────────────────
        connect(btnConfig, &QPushButton::clicked, [this]() {
            RuleEditor dlg(rawTable.columnNames(), this);
            if (!currentRule.isEmpty()) dlg.setRule(currentRule);
            if (dlg.exec() == QDialog::Accepted) {
                currentRule = dlg.getRule();
                btnRun->setEnabled(!currentRule.isEmpty());
                statusLabel->setText(
                    QString("✅ 规则已配置：%1 条公式，统计列 %2 个，分组列：%3")
                    .arg(currentRule.formulas.size())
                    .arg(currentRule.stat.statCols.size())
                    .arg(currentRule.stat.groupCol.isEmpty()
                         ? "无" : currentRule.stat.groupCol));
            }
        });

        // ── 执行计算 ──────────────────────────────
        connect(btnRun, &QPushButton::clicked, [this]() {
            if (rawTable.isEmpty() || currentRule.isEmpty()) return;

            // 把 ProcessRule 转换成 Formula 列表
            QVector<Formula> formulas;
            for (const auto& fr : currentRule.formulas) {
                if      (fr.type == "divide")
                    formulas << FormulaEngine::divide(fr.name, fr.colA, fr.colB);
                else if (fr.type == "subtract")
                    formulas << FormulaEngine::subtract(fr.name, fr.colA, fr.colB);
                else if (fr.type == "add")
                    formulas << FormulaEngine::add(fr.name, fr.colA, fr.colB);
                else if (fr.type == "multiply")
                    formulas << FormulaEngine::multiply(fr.name, fr.colA, fr.colB);
                else if (fr.type == "classify")
                    formulas << FormulaEngine::classify(
                        fr.name, fr.colA, fr.conditions);
            }

            resultTable = FormulaEngine::apply(rawTable, formulas);

            // 统计
            statReport = StatReport();
            if (!currentRule.stat.statCols.isEmpty())
                statReport = StatEngine::report(
                    resultTable,
                    currentRule.stat.statCols,
                    currentRule.stat.groupCol);

            // 显示
            QString out;
            out += "══════════════════════════════════════\n";
            out += "  计算结果\n";
            out += "══════════════════════════════════════\n";
            for (const auto& col : resultTable.columns)
                out += col.name.leftJustified(14);
            out += "\n" + QString("─").repeated(resultTable.columnCount()*14) + "\n";
            for (int r = 0; r < resultTable.rowCount(); ++r) {
                for (int c = 0; c < resultTable.columnCount(); ++c)
                    out += resultTable.value(r,c).toString().leftJustified(14);
                out += "\n";
            }

            if (!statReport.colStats.isEmpty()) {
                out += "\n══════════════════════════════════════\n";
                out += "  整体统计\n";
                out += "══════════════════════════════════════\n";
                out += QString("列名").leftJustified(16)
                     + QString("合计").leftJustified(12)
                     + QString("均值").leftJustified(12)
                     + QString("最大").leftJustified(12)
                     + QString("最小").leftJustified(12) + "\n";
                out += QString("─").repeated(64) + "\n";
                for (const auto& s : statReport.colStats)
                    out += s.name.leftJustified(16)
                         + QString::number(s.sum,'f',1).leftJustified(12)
                         + QString::number(s.mean,'f',1).leftJustified(12)
                         + QString::number(s.count>0?s.max:0,'f',1).leftJustified(12)
                         + QString::number(s.count>0?s.min:0,'f',1).leftJustified(12)+"\n";
            }

            const DataTable& gt = statReport.groupTable;
            if (!gt.isEmpty()) {
                out += "\n══════════════════════════════════════\n";
                out += "  分组统计\n";
                out += "══════════════════════════════════════\n";
                for (const auto& col : gt.columns)
                    out += col.name.leftJustified(14);
                out += "\n" + QString("─").repeated(gt.columnCount()*14) + "\n";
                for (int r = 0; r < gt.rowCount(); ++r) {
                    for (int c = 0; c < gt.columnCount(); ++c)
                        out += gt.value(r,c).toString().leftJustified(14);
                    out += "\n";
                }
            }

            output->setText(out);
            statusLabel->setText("✅ 计算完成");
            btnExportXlsx->setEnabled(true);
            btnExportPdf ->setEnabled(true);
        });

        // ── 导出 xlsx ─────────────────────────────
        connect(btnExportXlsx, &QPushButton::clicked, [this]() {
            QString path = QFileDialog::getSaveFileName(
                this, "保存 xlsx", "", "Excel Files (*.xlsx)");
            if (path.isEmpty()) return;

            // 找分级列 → 自动添加颜色
            WriteConfig cfg;
            cfg.boldHeader = true; cfg.autoColWidth = true;
            for (const auto& fr : currentRule.formulas) {
                if (fr.type == "classify") {
                    ValueColorRule vcr;
                    vcr.colName = fr.name;
                    for (const auto& cond : fr.conditions) {
                        QString label = cond.result.toString();
                        // 简单配色：根据顺序自动分配绿/黄/红
                        QColor c;
                        int idx = fr.conditions.indexOf(cond);
                        QColor colors[] = {QColor(0,176,80), QColor(255,192,0), QColor(255,80,80)};
                        vcr.colorMap[label] = colors[qMin(idx, 2)];
                    }
                    cfg.colorRules << vcr;
                }
            }

            QString err;
            QVector<DataTable> sheets = {resultTable};
            if (!statReport.groupTable.isEmpty())
                sheets << statReport.groupTable;
            if (XlsxWriter::writeSheets(sheets, path, cfg, &err))
                QMessageBox::information(this, "导出成功", "已保存：\n" + path);
            else
                QMessageBox::warning(this, "导出失败", err);
        });

        // ── 导出 PDF ──────────────────────────────
        connect(btnExportPdf, &QPushButton::clicked, [this]() {
            QString path = QFileDialog::getSaveFileName(
                this, "保存 PDF", "", "PDF Files (*.pdf)");
            if (path.isEmpty()) return;
            QString err;
            if (PdfExporter::exportReport(resultTable, statReport, path, &err))
                QMessageBox::information(this, "导出成功", "已保存：\n" + path);
            else
                QMessageBox::warning(this, "导出失败", err);
        });
    }

private:
    QLabel*      statusLabel;
    QPushButton* btnOpen;
    QPushButton* btnConfig;
    QPushButton* btnRun;
    QPushButton* btnExportXlsx;
    QPushButton* btnExportPdf;
    QTextEdit*   output;

    DataTable    rawTable;
    DataTable    resultTable;
    StatReport   statReport;
    ProcessRule  currentRule;
};

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    MainWindow w;
    w.show();
    return app.exec();
}