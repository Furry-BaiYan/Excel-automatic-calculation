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
static const int ROW_H  = 60;

static QColor textColorFor(const QColor& bg) {
    return (bg.red()*299+bg.green()*587+bg.blue()*114)>128000?Qt::black:Qt::white;
}

// ── 章节标题 ──────────────────────────────────
static int drawSection(QPainter& p, int x, int y, int pageW,
                       const QString& title) {
    QFont f; f.setFamily("Microsoft YaHei"); f.setPointSize(13); f.setBold(true);
    p.setFont(f); p.setPen(QColor(31,73,125));
    p.drawText(x, y+50, title);
    p.setPen(QPen(QColor(68,114,196),3));
    p.drawLine(x, y+56, pageW-x, y+56);
    return y+68;
}

// ── 小标题（用于指标分组）────────────────────
static int drawSubTitle(QPainter& p, int x, int y, const QString& title) {
    QFont f; f.setFamily("Microsoft YaHei"); f.setPointSize(10); f.setBold(true);
    p.setFont(f); p.setPen(QColor(31,73,125));
    p.drawText(x, y+18, "▸ " + title);
    return y+22;
}

// ── 通用表格绘制 ──────────────────────────────
static int drawTable(QPainter& p,
                     const QStringList& headers,
                     const QVector<QStringList>& rows,
                     int x, int y, int tableW, int pageH,
                     QPdfWriter* writer,
                     int fontSize=9, int hdrH=72,
                     const QMap<int, QMap<QString,QColor>>& colorRules={},
                     bool equalCols=false) { 
    int nc=headers.size();
    if (nc==0) return y;

    // 列宽按内容分配
    QVector<int> cw(nc);
    if (equalCols) {
        // 等宽：所有列宽相同
        int each = tableW / nc;
        for (int c=0;c<nc;++c) cw[c]=each;
        cw.last() += tableW - each*nc; // 补尾差
    } else {
        QVector<int> maxLen(nc,2);
        for (int c=0;c<nc;++c) {
            maxLen[c]=qMax(maxLen[c],headers[c].length());
            for (const auto& r:rows) if(c<r.size()) maxLen[c]=qMax(maxLen[c],r[c].length());
        }
        int total=0; for(int l:maxLen) total+=l;
        for(int c=0;c<nc;++c) cw[c]=tableW*maxLen[c]/total;
        int s=0; for(int w:cw) s+=w; cw.last()+=tableW-s;
    }

    auto drawHdr=[&](){
        int cx=x;
        QFont hf; hf.setFamily("Microsoft YaHei"); hf.setPointSize(fontSize); hf.setBold(true);
        p.setFont(hf);
        for(int c=0;c<nc;++c){
            QRect cell(cx,y,cw[c],hdrH);
            p.fillRect(cell,HDR_BG); p.setPen(BORDER); p.drawRect(cell);
            p.setPen(HDR_FG);
            p.drawText(cell.adjusted(3,3,-3,-3),Qt::AlignCenter|Qt::TextWordWrap,headers[c]);
            cx+=cw[c];
        }
    };
    drawHdr(); y+=hdrH;

    QFont df; df.setFamily("Microsoft YaHei"); df.setPointSize(fontSize);
    p.setFont(df);

    for(int ri=0;ri<rows.size();++ri){
        if(y+ROW_H>pageH-MARGIN){
            writer->newPage(); y=MARGIN;
            drawHdr(); y+=hdrH; p.setFont(df);
        }
        QColor rowBg=(ri%2==0)?Qt::white:ALT_ROW;
        int cx=x;
        for(int c=0;c<nc;++c){
            QString val=c<rows[ri].size()?rows[ri][c]:"";
            QColor bg=rowBg;
            if(colorRules.contains(c)&&colorRules[c].contains(val)) bg=colorRules[c][val];
            QRect cell(cx,y,cw[c],ROW_H);
            p.fillRect(cell,bg); p.setPen(BORDER); p.drawRect(cell);
            p.setPen(textColorFor(bg));
            p.drawText(cell.adjusted(5,0,-5,0),Qt::AlignVCenter|Qt::AlignLeft,val);
            cx+=cw[c];
        }
        y+=ROW_H;
    }
    return y;
}

// ── 主导出函数 ────────────────────────────────
bool PdfExporter::exportReport(const DataTable& dt,
                                const StatReport& rpt,
                                const QString& path,
                                const QString& title,
                                QString* err) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QPdfWriter writer(path);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setPageOrientation(QPageLayout::Landscape);
    writer.setResolution(300);

    QPainter p(&writer);
    if(!p.isActive()){if(err)*err="PDF 初始化失败";return false;}

    int PW=p.window().width(), PH=p.window().height(), TW=PW-MARGIN*2;

    // ── 报告标题 ──────────────────────────────
    QString reportTitle=title.trimmed().isEmpty()
        ? dt.name+"  数据报告" : dt.name+"  "+title.trimmed();

    p.fillRect(0,0,PW,128,QColor(31,73,125));
    QFont tf; tf.setFamily("Microsoft YaHei"); tf.setPointSize(20); tf.setBold(true);
    p.setFont(tf); p.setPen(Qt::white);
    p.drawText(QRect(MARGIN,0,PW-MARGIN*2,100),Qt::AlignVCenter|Qt::AlignLeft,reportTitle);
    QFont sf; sf.setFamily("Microsoft YaHei"); sf.setPointSize(9);
    p.setFont(sf);
    p.drawText(QRect(0,92,PW-MARGIN,30),Qt::AlignRight|Qt::AlignVCenter,
               "生成时间："+QDateTime::currentDateTime().toString("yyyy-MM-dd  hh:mm:ss  "));

    int y=148;

    // ── 一、数据明细 ──────────────────────────
    y=drawSection(p,MARGIN,y,PW,"一、数据明细");

    // 自动检测分级列颜色
    QMap<int,QMap<QString,QColor>> scoreColors;
    QList<QColor> palette={
        QColor(0,176,80),QColor(255,192,0),QColor(255,80,80),
        QColor(100,149,237),QColor(200,200,200)};
    for(int ci=0;ci<dt.columnCount();++ci){
        if(dt.columns[ci].type==ColumnType::Text&&ci>0){
            QSet<QString> vals;
            for(int r=0;r<dt.rowCount();++r) vals.insert(dt.value(r,ci).toString());
            if(vals.size()>=2&&vals.size()<=6){
                QMap<QString,QColor> cm; int idx=0;
                QStringList sorted=vals.values(); std::sort(sorted.begin(),sorted.end());
                for(const auto& v:sorted) cm[v]=palette[idx++%palette.size()];
                scoreColors[ci]=cm;
            }
        }
    }

    QVector<QStringList> dataRows;
    for(int r=0;r<dt.rowCount();++r){
        QStringList row;
        for(int c=0;c<dt.columnCount();++c) row<<dt.value(r,c).toString();
        dataRows<<row;
    }
    y=drawTable(p,dt.columnNames(),dataRows,MARGIN,y,TW,PH,&writer,10,72,scoreColors);
    y+=20;

    // ── 二、整体统计 ──────────────────────────
    if(!rpt.colStats.isEmpty()){
        if(y+180>PH-MARGIN){writer.newPage();y=MARGIN;}
        y=drawSection(p,MARGIN,y,PW,"二、整体统计");
        QVector<QStringList> sr;
        for(const auto& s:rpt.colStats)
            sr<<QStringList{s.name,
                QString::number(s.sum,'f',1),
                QString::number(s.mean,'f',1),
                QString::number(s.count>0?s.max:0,'f',1),
                QString::number(s.count>0?s.min:0,'f',1)};
        y=drawTable(p,{"指标","合计","均值","最大","最小"},
                    sr,MARGIN,y,TW/2,PH,&writer,10,72);
        y+=20;
    }

    // ── 三、分组统计 ──────────────────────────
    const DataTable& gt=rpt.groupTable;
    if(!gt.isEmpty()){
        if(y+180>PH-MARGIN){writer.newPage();y=MARGIN;}
        y=drawSection(p,MARGIN,y,PW,"三、分组统计");

        // 分组列颜色
        QMap<int,QMap<QString,QColor>> grpColors;
        {
            QSet<QString> vals;
            for(int r=0;r<gt.rowCount();++r) vals.insert(gt.value(r,0).toString());
            QMap<QString,QColor> cm; int idx=0;
            QStringList sorted=vals.values(); std::sort(sorted.begin(),sorted.end());
            for(const auto& v:sorted) cm[v]=palette[idx++%palette.size()];
            grpColors[0]=cm;
        }

        int nc=gt.columnCount();
        if(nc<=10){
            // 列数少：直接一张表
            QVector<QStringList> grpRows;
            for(int r=0;r<gt.rowCount();++r){
                QStringList row;
                for(int c=0;c<nc;++c) row<<gt.value(r,c).toString();
                grpRows<<row;
            }
            y=drawTable(p,gt.columnNames(),grpRows,MARGIN,y,TW,PH,&writer,9,72,grpColors,true);
        } else {
            // 列数多：按指标拆分，每个指标一张表，顺序排列（不并排）
            QStringList baseCols;
            QMap<QString,QStringList> metricCols;
            QStringList metricOrder;
            for(const auto& col:gt.columnNames()){
                if(!col.contains("_")) baseCols<<col;
                else{
                    QString m=col.left(col.lastIndexOf("_"));
                    if(!metricCols.contains(m)) metricOrder<<m;
                    metricCols[m]<<col;
                }
            }

            for(const auto& metric:metricOrder){
                if(y+180>PH-MARGIN){writer.newPage();y=MARGIN;}

                y=drawSubTitle(p,MARGIN,y,metric);
                y+=4;

                // 列名：基础列 + 简化后缀（合计/均值/最大/最小）
                QStringList hdr=baseCols;
                for(const auto& c:metricCols[metric])
                    hdr<<c.mid(c.lastIndexOf("_")+1);

                // 数据行
                QVector<int> idxs;
                for(const auto& cn:baseCols+metricCols[metric]) idxs<<gt.columnIndex(cn);
                QVector<QStringList> grpRows;
                for(int r=0;r<gt.rowCount();++r){
                    QStringList row;
                    for(int idx:idxs) row<<(idx>=0?gt.value(r,idx).toString():"");
                    grpRows<<row;
                }
                y=drawTable(p,hdr,grpRows,MARGIN,y,TW*2/3,PH,&writer,9,68,grpColors,true);
                y+=14; // 表间小间距
            }
        }
    }

    p.end();
    qDebug()<<"[PdfExporter] 导出成功:"<<path;
    return true;
}