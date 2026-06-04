#include "FormulaParser.h"
#include "ExprEvaluator.h"
#include <QDebug>
#include <cmath>

FormulaParser::Result FormulaParser::parse(const QString& text,
                                             QStringList knownCols) {
    Result result;

    // ── 预处理：合并续行、去掉行内注释 ───────────────
    QStringList lines;
    QString current;

    for (const auto& rawLine : text.split('\n')) {
        QString line = rawLine.trimmed();

        // 去行内注释（跳过字符串内的 #）
        {
            bool inStr=false; QChar sc;
            for (int i=0;i<line.size();++i) {
                QChar c=line[i];
                if (!inStr&&(c=='"'||c=='\'')) {inStr=true;sc=c;}
                else if (inStr&&c==sc)          {inStr=false;}
                else if (!inStr&&c=='#')        {line=line.left(i).trimmed();break;}
            }
        }
        if (line.isEmpty()) {
            if (!current.isEmpty()) {lines<<current;current.clear();}
            continue;
        }

        // 判断是否新公式的开头
        bool isNew=false;
        for (int i=0;i<line.size();++i) {
            QChar c=line[i];
            if (c=='='&&
                (i==0||(line[i-1]!='>'&&line[i-1]!='<'&&line[i-1]!='!'&&line[i-1]!='='))&&
                (i+1>=line.size()||line[i+1]!='=')) {
                QString lhs=line.left(i).trimmed();
                if (!lhs.isEmpty()&&lhs[0]!='('&&lhs[0]!='+'&&
                    lhs[0]!='-'&&lhs[0]!='*'&&lhs[0]!='/') {isNew=true;}
                break;
            }
        }
        if (isNew) { if(!current.isEmpty()) lines<<current; current=line; }
        else         current+=" "+line;  // 续行
    }
    if (!current.isEmpty()) lines<<current;

    // ── 逐行解析 ──────────────────────────────────
    int lineNo=0;
    for (const auto& line : lines) {
        ++lineNo;
        if (line.trimmed().isEmpty()) continue;

        int eqPos=-1;
        for (int i=0;i<line.size();++i) {
            QChar c=line[i];
            if (c=='='&&
                (i==0||(line[i-1]!='>'&&line[i-1]!='<'&&line[i-1]!='!'&&line[i-1]!='='))&&
                (i+1>=line.size()||line[i+1]!='=')) {eqPos=i;break;}
        }
        if (eqPos<0) {
            result.errors<<QString("第%1行缺少 '='：%2").arg(lineNo).arg(line);
            continue;
        }

        QString newCol=line.left(eqPos).trimmed();
        QString expr  =line.mid(eqPos+1).trimmed();
        if (newCol.isEmpty()) {result.errors<<"列名为空："+line;continue;}
        if (expr.isEmpty())   {result.errors<<"表达式为空："+line;continue;}

        auto compiled=ExprEvaluator::compile(expr, knownCols);
        if (!compiled.ok()) {
            result.errors<<QString("'%1' 解析错误：%2").arg(newCol).arg(compiled.error);
            continue;
        }

        Formula f;
        f.name=newCol;
        f.type=compiled.isNumeric?ColumnType::Double:ColumnType::Text;

        auto evalFn=compiled.eval;
        f.calc=[evalFn](const DataTable& table, int rowIdx)->QVariant {
            ExprEvaluator::Row rowMap;

            // 当前行的列值
            for (int c=0;c<table.columnCount();++c)
                rowMap[table.columns[c].name]=table.value(rowIdx,c);

            // ── 预计算列聚合（让 MAX/MIN/SUM/AVERAGE 等支持跨行计算）──
            for (int ci=0;ci<table.columnCount();++ci) {
                QString cn=table.columns[ci].name;
                double sum=0,mx=-1e18,mn=1e18,s2=0; int cnt=0;
                for (int ri=0;ri<table.rowCount();++ri) {
                    bool ok; double v=table.value(ri,ci).toString().toDouble(&ok);
                    if(ok){sum+=v;if(v>mx)mx=v;if(v<mn)mn=v;s2+=v*v;cnt++;}
                }
                if (cnt>0) {
                    double avg=sum/cnt;
                    double var=cnt>1?(s2-sum*sum/cnt)/(cnt-1):0;
                    rowMap["__AGG_MAX_"+cn]  = mx;
                    rowMap["__AGG_MIN_"+cn]  = mn;
                    rowMap["__AGG_SUM_"+cn]  = sum;
                    rowMap["__AGG_AVG_"+cn]  = avg;
                    rowMap["__AGG_COUNT_"+cn]= (double)cnt;
                    rowMap["__AGG_STDEV_"+cn]= var>=0?std::sqrt(var):0.0;
                }
            }

            return evalFn(rowMap);
        };

        result.formulas<<f;
        if (!knownCols.contains(newCol)) knownCols<<newCol;
        qDebug()<<"[FormulaParser] 编译成功:"<<newCol<<"="<<expr.left(60);
    }
    return result;
}