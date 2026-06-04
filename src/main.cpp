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
#include "config/FormulaParser.h"
#include "config/RuleEditor.h"
#include "config/ProcessRule.h"

class MainWindow : public QMainWindow {
public:
    MainWindow(QWidget* parent = nullptr) : QMainWindow(parent) {
        setWindowTitle("Excel Processor");
        setMinimumSize(960, 700);

        auto* central    = new QWidget(this);
        auto* mainLayout = new QVBoxLayout(central);

        statusLabel = new QLabel("请打开一个 xlsx 文件", this);
        statusLabel->setStyleSheet(
            "background:#1e3a5f;color:white;padding:6px 12px;font-size:13px;");

        auto* btnRow = new QHBoxLayout;
        btnOpen       = new QPushButton("📂 打开 xlsx",  this);
        btnConfig     = new QPushButton("⚙️  配置规则",   this);
        btnRun        = new QPushButton("▶  执行计算",    this);
        btnExportXlsx = new QPushButton("💾 导出 xlsx",  this);
        btnExportPdf  = new QPushButton("📄 导出 PDF",   this);

        btnConfig->setEnabled(false);
        btnRun->setEnabled(false);
        btnExportXlsx->setEnabled(false);
        btnExportPdf ->setEnabled(false);
        btnRun->setStyleSheet(
            "background:#217346;color:white;font-weight:bold;padding:6px 18px;");

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
                this, "选择 Excel 文件", "", "Excel Files (*.xlsx)");
            if (path.isEmpty()) return;

            QString err;
            rawTable = XlsxReader::readFile(path, {}, &err);
            if (!err.isEmpty()) {
                statusLabel->setText("❌ 读取失败：" + err); return;
            }

            formulaText.clear();
            statConfig  = {"", {}};
            reportTitle.clear();
            btnRun->setEnabled(false);
            btnExportXlsx->setEnabled(false);
            btnExportPdf ->setEnabled(false);

            statusLabel->setText(
                QString("✅ %1  |  %2行 × %3列  |  列：%4")
                .arg(rawTable.name).arg(rawTable.rowCount())
                .arg(rawTable.columnCount())
                .arg(rawTable.columnNames().join("、")));

            output->setText(
                "文件已读取，请点击【⚙️ 配置规则】设置计算逻辑。\n\n"
                "探测到的列：\n" + rawTable.columnNames().join("\n"));
            btnConfig->setEnabled(true);
        });

        // ── 配置规则 ──────────────────────────────
        connect(btnConfig, &QPushButton::clicked, [this]() {
            RuleEditor dlg(rawTable.columnNames(), this);

            // 传入前5行样本数据给 AI 分析
            QVector<QStringList> sampleRows;
            for (int r = 0; r < qMin(5, rawTable.rowCount()); ++r) {
                QStringList row;
                for (int c = 0; c < rawTable.columnCount(); ++c)
                    row << rawTable.value(r, c).toString();
                sampleRows << row;
            }
            dlg.setSampleData(sampleRows);

            // 恢复上次规则
            if (!formulaText.isEmpty()) dlg.setFormulaText(formulaText);
            dlg.setStatConfig(statConfig.first, statConfig.second);
            if (!reportTitle.isEmpty()) dlg.setReportTitle(reportTitle);

            if (dlg.exec() == QDialog::Accepted) {
                formulaText = dlg.getFormulaText();
                statConfig  = dlg.getStatConfig();
                reportTitle = dlg.getReportTitle();
                btnRun->setEnabled(!formulaText.trimmed().isEmpty());
                statusLabel->setText("✅ 规则已配置，点【▶ 执行计算】运行");
            }
        });

        // ── 执行计算 ──────────────────────────────
        connect(btnRun, &QPushButton::clicked, [this]() {
            auto parsed = FormulaParser::parse(formulaText, rawTable.columnNames());
            if (!parsed.ok()) {
                output->setText("❌ 公式错误：\n" + parsed.errors.join("\n"));
                return;
            }

            resultTable = FormulaEngine::apply(rawTable, parsed.formulas);
            statReport  = StatReport();
            if (!statConfig.second.isEmpty())
                statReport = StatEngine::report(
                    resultTable, statConfig.second, statConfig.first);

            // 显示结果
            QString out;
            out += "══════════════════════════════════════\n";
            out += "   计算结果\n";
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
                out += "   整体统计\n";
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

            const auto& gt = statReport.groupTable;
            if (!gt.isEmpty()) {
                out += "\n══════════════════════════════════════\n";
                out += "   分组统计\n";
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
            WriteConfig cfg; cfg.boldHeader=true; cfg.autoColWidth=true;
            QString err;
            QVector<DataTable> sheets = {resultTable};
            if (!statReport.groupTable.isEmpty()) sheets << statReport.groupTable;
            if (XlsxWriter::writeSheets(sheets, path, cfg, &err))
                QMessageBox::information(this, "✅ 导出成功", "已保存：\n" + path);
            else
                QMessageBox::warning(this, "❌ 导出失败", err);
        });

        // ── 导出 PDF ──────────────────────────────
        connect(btnExportPdf, &QPushButton::clicked, [this]() {
            QString path = QFileDialog::getSaveFileName(
                this, "保存 PDF", "", "PDF Files (*.pdf)");
            if (path.isEmpty()) return;
            QString err;
            if (PdfExporter::exportReport(resultTable, statReport, path, reportTitle, &err))
                QMessageBox::information(this, "✅ 导出成功", "已保存：\n" + path);
            else
                QMessageBox::warning(this, "❌ 导出失败", err);
        });
    }

private:
    QLabel*      statusLabel;
    QPushButton* btnOpen, *btnConfig, *btnRun, *btnExportXlsx, *btnExportPdf;
    QTextEdit*   output;

    DataTable    rawTable, resultTable;
    StatReport   statReport;
    QString      formulaText;
    QString      reportTitle;
    QPair<QString, QStringList> statConfig;
};

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    MainWindow w;
    w.show();
    return app.exec();
}