#include "PdfExporter.h"
#include <QPdfWriter>
#include <QPainter>
#include <QFont>
#include <QColor>
#include <QDateTime>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

static const QColor HDR_BG (68,  114, 196);
static const QColor HDR_FG (255, 255, 255);
static const QColor ALT_ROW(235, 241, 250);
static const QColor BORDER (180, 180, 180);
static const int MARGIN = 150;
static const int ROW_H  = 65;

static QColor textColorFor(const QColor& bg) {
    return (bg.red()*299 + bg.green()*587 + bg.blue()*114) > 128000
           ? Qt::black : Qt::white;
}

// ── 章节标题 ───────────────────────────────────
static int drawSection(QPainter& p, int x, int y, int pageW,
                       const QString& title) {
    QFont f; f.setFamily("Microsoft YaHei"); f.setPointSize(13); f.setBold(true);
    p.setFont(f);
    p.setPen(QColor(31, 73, 125));
    p.drawText(x, y + 52, title);
    p.setPen(QPen(QColor(68,114,196), 3));
    p.drawLine(x, y + 58, pageW - x, y + 58);
    return y + 72;
}

// ── 通用表格绘制 ───────────────────────────────
// fontSize: 表头和数据的字号
// hdrH: 表头行高（列多时传大值）
static int drawTable(QPainter& p,
                     const QStringList& headers,
                     const QVector<QStringList>& rows,
                     int x, int y, int tableW, int pageH,
                     QPdfWriter* writer,
                     int fontSize = 9, int hdrH = 80,
                     const QMap<int, QMap<QString,QColor>>& colorRules = {}) {
    int nc = headers.size();
    if (nc == 0) return y;

    // 动态列宽：按内容最大长度分配
    QVector<int> maxLen(nc, 2);
    for (int c = 0; c < nc; ++c) {
        maxLen[c] = qMax(maxLen[c], headers[c].length());
        for (const auto& r : rows)
            if (c < r.size()) maxLen[c] = qMax(maxLen[c], r[c].length());
    }
    int total = 0; for (int l : maxLen) total += l;
    QVector<int> cw(nc);
    for (int c = 0; c < nc; ++c)
        cw[c] = tableW * maxLen[c] / total;
    // 补齐尾差
    int s = 0; for (int w : cw) s += w;
    cw.last() += tableW - s;

    auto drawHdr = [&]() {
        int cx = x;
        QFont hf; hf.setFamily("Microsoft YaHei");
        hf.setPointSize(fontSize); hf.setBold(true);
        p.setFont(hf);
        for (int c = 0; c < nc; ++c) {
            QRect cell(cx, y, cw[c], hdrH);
            p.fillRect(cell, HDR_BG);
            p.setPen(BORDER); p.drawRect(cell);
            p.setPen(HDR_FG);
            p.drawText(cell.adjusted(3,3,-3,-3),
                       Qt::AlignCenter | Qt::TextWordWrap, headers[c]);
            cx += cw[c];
        }
    };

    drawHdr();
    y += hdrH;

    QFont df; df.setFamily("Microsoft YaHei"); df.setPointSize(fontSize);
    p.setFont(df);

    for (int ri = 0; ri < rows.size(); ++ri) {
        if (y + ROW_H > pageH - MARGIN) {
            writer->newPage(); y = MARGIN;
            drawHdr(); y += hdrH;
            p.setFont(df);
        }
        QColor rowBg = (ri % 2 == 0) ? Qt::white : ALT_ROW;
        int cx = x;
        for (int c = 0; c < nc; ++c) {
            QString val = c < rows[ri].size() ? rows[ri][c] : "";
            QColor bg = rowBg;
            if (colorRules.contains(c) && colorRules[c].contains(val))
                bg = colorRules[c][val];
            QRect cell(cx, y, cw[c], ROW_H);
            p.fillRect(cell, bg);
            p.setPen(BORDER); p.drawRect(cell);
            p.setPen(textColorFor(bg));
            p.drawText(cell.adjusted(5,0,-5,0),
                       Qt::AlignVCenter | Qt::AlignLeft, val);
            cx += cw[c];
        }
        y += ROW_H;
    }
    return y;
}

// ── 主导出 ─────────────────────────────────────
bool PdfExporter::exportReport(const DataTable&  dt,
                                const StatReport& rpt,
                                const QString&    path,
                                QString*          err) {
    QDir().mkpath(QFileInfo(path).absolutePath());

    QPdfWriter writer(path);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setPageOrientation(QPageLayout::Landscape);
    writer.setResolution(300);

    QPainter p(&writer);
    if (!p.isActive()) { if (err) *err = "PDF 初始化失败"; return false; }

    int PW = p.window().width();
    int PH = p.window().height();
    int TW = PW - MARGIN * 2;

    // ── 报告头 ──
    p.fillRect(0, 0, PW, 128, QColor(31,73,125));
    QFont tf; tf.setFamily("Microsoft YaHei"); tf.setPointSize(20); tf.setBold(true);
    p.setFont(tf); p.setPen(Qt::white);
    p.drawText(QRect(MARGIN, 0, PW-MARGIN*2, 100),
               Qt::AlignVCenter|Qt::AlignLeft, dt.name + "  销售数据报告");
    QFont sf; sf.setFamily("Microsoft YaHei"); sf.setPointSize(9);
    p.setFont(sf);
    p.drawText(QRect(0, 92, PW-MARGIN, 30), Qt::AlignRight|Qt::AlignVCenter,
               "生成时间：" + QDateTime::currentDateTime()
                              .toString("yyyy-MM-dd  hh:mm:ss  "));

    int y = 148;

    // ── 一、数据明细 ──
    y = drawSection(p, MARGIN, y, PW, "一、数据明细");

    QMap<int, QMap<QString,QColor>> scoreColors;
    int si = dt.columnIndex("绩效评分");
    if (si >= 0) scoreColors[si] = {
        {"优秀",   QColor(0,176,80)},
        {"良好",   QColor(255,192,0)},
        {"待改进", QColor(255,80,80)}
    };

    QVector<QStringList> dataRows;
    for (int r = 0; r < dt.rowCount(); ++r) {
        QStringList row;
        for (int c = 0; c < dt.columnCount(); ++c)
            row << dt.value(r, c).toString();
        dataRows << row;
    }
    y = drawTable(p, dt.columnNames(), dataRows,
                  MARGIN, y, TW, PH, &writer, 10, 80, scoreColors);
    y += 28;

    // ── 二、整体统计 ──
    if (y + 220 > PH - MARGIN) { writer.newPage(); y = MARGIN; }
    y = drawSection(p, MARGIN, y, PW, "二、整体统计");
    {
        QVector<QStringList> sr;
        for (const auto& s : rpt.colStats)
            sr << QStringList{s.name,
                QString::number(s.sum,'f',1),
                QString::number(s.mean,'f',1),
                QString::number(s.count>0 ? s.max : 0.0,'f',1),
                QString::number(s.count>0 ? s.min : 0.0,'f',1)};
        y = drawTable(p, {"指标","合计","均值","最大","最小"},
                      sr, MARGIN, y, TW/2, PH, &writer, 10, 80);
    }
    y += 28;

    // ── 三、分组统计（拆成3张小表）──
    const DataTable& gt = rpt.groupTable;
    if (!gt.isEmpty()) {
        if (y + 220 > PH - MARGIN) { writer.newPage(); y = MARGIN; }
        y = drawSection(p, MARGIN, y, PW, "三、分组统计（按绩效评分）");

        QMap<int, QMap<QString,QColor>> grpColors;
        grpColors[0] = {
            {"优秀",   QColor(0,176,80)},
            {"良好",   QColor(255,192,0)},
            {"待改进", QColor(255,80,80)}
        };

        // 找出基础列（绩效评分、数量）和每个指标的四列
        // 格式："指标名_合计" "指标名_均值" "指标名_最大" "指标名_最小"
        QStringList allCols = gt.columnNames();
        QStringList baseCols; // 绩效评分、数量
        QMap<QString, QStringList> metricCols; // 指标 → [合计,均值,最大,最小]
        QStringList metricOrder;

        for (const auto& col : allCols) {
            if (!col.contains("_")) {
                baseCols << col;
            } else {
                QString metric = col.left(col.lastIndexOf("_"));
                if (!metricCols.contains(metric)) metricOrder << metric;
                metricCols[metric] << col;
            }
        }

        // 数据行（每行只取需要的列）
        auto makeRows = [&](const QStringList& colNames) {
            QVector<QStringList> rows;
            QVector<int> idxs;
            for (const auto& cn : colNames) idxs << gt.columnIndex(cn);
            for (int r = 0; r < gt.rowCount(); ++r) {
                QStringList row;
                for (int idx : idxs)
                    row << (idx>=0 ? gt.value(r,idx).toString() : "");
                rows << row;
            }
            return rows;
        };

        // 每个指标单独画一张小表（放在同一行，左右并排）
        int halfW = (TW - 20) / 2;
        int startY = y;
        int maxY = y;
        bool leftSide = true;
        int curX = MARGIN;

        for (const auto& metric : metricOrder) {
            QStringList hdr = baseCols + metricCols[metric];
            auto rows = makeRows(hdr);

            // 简化显示：把列名后缀去掉（只保留"合计""均值""最大""最小"）
            QStringList shortHdr = baseCols;
            for (const auto& c : metricCols[metric])
                shortHdr << c.mid(c.lastIndexOf("_") + 1);

            // 小表标题
            QFont mf; mf.setFamily("Microsoft YaHei"); mf.setPointSize(10); mf.setBold(true);
            p.setFont(mf); p.setPen(QColor(31,73,125));
            p.drawText(curX, startY + 20, "【" + metric + "】");

            int tableY = startY + 28;
            int endY = drawTable(p, shortHdr, rows,
                                 curX, tableY, halfW, PH, &writer,
                                 9, 75, grpColors);
            maxY = qMax(maxY, endY);

            if (leftSide) {
                curX = MARGIN + halfW + 20;
                leftSide = false;
            } else {
                startY = maxY + 20;
                curX = MARGIN;
                leftSide = true;
            }
        }
        y = maxY;
    }

    p.end();
    qDebug() << "[PdfExporter] 导出成功:" << path;
    return true;
}