#include "ExprEvaluator.h"
#include <QDate>
#include <QDateTime>
#include <QRegularExpression>
#include <QDebug>
#include <cmath>
#include <algorithm>

// ══════════════════════════════════════════════
//  工具
// ══════════════════════════════════════════════
static double toNum(const QVariant& v) {
    bool ok; double d = v.toString().toDouble(&ok); return ok ? d : 0.0;
}

// ══════════════════════════════════════════════
//  函数注册表
// ══════════════════════════════════════════════
QString ExprEvaluator::normKey(const QString& name) {
    QString r;
    for (const QChar& c : name)
        r += (c.isLetter() && c.unicode() < 128) ? c.toUpper() : c;
    return r;
}

QMap<QString, ExprEvaluator::EagerFn>& ExprEvaluator::registry() {
    static QMap<QString, EagerFn> reg;
    return reg;
}

void ExprEvaluator::registerFunction(const QString& name, EagerFn fn) {
    registry()[normKey(name)] = fn;
}

void ExprEvaluator::initRegistry() {
    static bool done = false;
    if (done) return;
    done = true;

    auto reg = [](const QString& name, EagerFn fn) {
        ExprEvaluator::registry()[ExprEvaluator::normKey(name)] = fn;
    };
    auto alias = [&](const QString& cn, const QString& en) {
        auto& r = ExprEvaluator::registry();
        QString k = ExprEvaluator::normKey(en);
        if (r.contains(k)) r[ExprEvaluator::normKey(cn)] = r[k];
    };

    // ════ 数学 ════════════════════════════════════
    reg("SUM",[](const QVector<QVariant>& a)->QVariant{
        double s=0; for(const auto& v:a) s+=toNum(v); return s;});
    alias("求和","SUM");

    reg("AVERAGE",[](const QVector<QVariant>& a)->QVariant{
        if(a.isEmpty()) return 0.0;
        double s=0; for(const auto& v:a) s+=toNum(v); return s/a.size();});
    alias("平均值","AVERAGE"); alias("平均","AVERAGE"); alias("均值","AVERAGE");
    reg("AVG",registry()[normKey("AVERAGE")]);

    reg("MAX",[](const QVector<QVariant>& a)->QVariant{
        if(a.isEmpty()) return 0.0;
        double m=toNum(a[0]);
        for(const auto& v:a){double d=toNum(v);if(d>m)m=d;} return m;});
    alias("最大值","MAX"); alias("最大","MAX");

    reg("MIN",[](const QVector<QVariant>& a)->QVariant{
        if(a.isEmpty()) return 0.0;
        double m=toNum(a[0]);
        for(const auto& v:a){double d=toNum(v);if(d<m)m=d;} return m;});
    alias("最小值","MIN"); alias("最小","MIN");

    reg("COUNT",[](const QVector<QVariant>& a)->QVariant{
        int c=0; for(const auto& v:a) if(!v.toString().trimmed().isEmpty()) c++;
        return (double)c;});
    alias("计数","COUNT"); alias("数量","COUNT");

    reg("PRODUCT",[](const QVector<QVariant>& a)->QVariant{
        double p=1.0; for(const auto& v:a) p*=toNum(v); return p;});
    alias("乘积","PRODUCT");

    reg("ROUND",[](const QVector<QVariant>& a)->QVariant{
        double v=toNum(a.value(0)),n=toNum(a.value(1));
        double f=std::pow(10.0,n); return std::round(v*f)/f;});
    alias("四舍五入","ROUND");

    reg("ROUNDUP",[](const QVector<QVariant>& a)->QVariant{
        double v=toNum(a.value(0)),n=toNum(a.value(1));
        double f=std::pow(10.0,n); return std::ceil(v*f)/f;});
    alias("向上取整","ROUNDUP"); alias("进一","ROUNDUP");

    reg("ROUNDDOWN",[](const QVector<QVariant>& a)->QVariant{
        double v=toNum(a.value(0)),n=toNum(a.value(1));
        double f=std::pow(10.0,n); return std::floor(v*f)/f;});
    alias("向下取整","ROUNDDOWN"); alias("截位","ROUNDDOWN");

    reg("ABS",[](const QVector<QVariant>& a)->QVariant{
        return std::abs(toNum(a.value(0)));});
    alias("绝对值","ABS");

    reg("SQRT",[](const QVector<QVariant>& a)->QVariant{
        double v=toNum(a.value(0)); return v>=0?std::sqrt(v):0.0;});
    alias("平方根","SQRT"); alias("开方","SQRT");

    reg("POWER",[](const QVector<QVariant>& a)->QVariant{
        return std::pow(toNum(a.value(0)),toNum(a.value(1)));});
    alias("幂","POWER"); alias("次方","POWER"); alias("乘方","POWER");
    reg("POW",registry()[normKey("POWER")]);

    reg("MOD",[](const QVector<QVariant>& a)->QVariant{
        double x=toNum(a.value(0)),y=toNum(a.value(1));
        return qFuzzyIsNull(y)?0.0:std::fmod(x,y);});
    alias("取余","MOD"); alias("余数","MOD"); alias("取模","MOD");

    reg("INT",[](const QVector<QVariant>& a)->QVariant{
        return std::floor(toNum(a.value(0)));});
    alias("取整","INT");

    reg("TRUNC",[](const QVector<QVariant>& a)->QVariant{
        double v=toNum(a.value(0)); int n=(int)toNum(a.value(1));
        double f=std::pow(10.0,n); return std::trunc(v*f)/f;});
    alias("截断","TRUNC"); alias("截尾","TRUNC");

    reg("FLOOR",[](const QVector<QVariant>& a)->QVariant{
        double v=toNum(a.value(0)),s=a.size()>=2?toNum(a.value(1)):1.0;
        return qFuzzyIsNull(s)?0.0:std::floor(v/s)*s;});
    alias("地板","FLOOR"); alias("下取整","FLOOR");

    reg("CEILING",[](const QVector<QVariant>& a)->QVariant{
        double v=toNum(a.value(0)),s=a.size()>=2?toNum(a.value(1)):1.0;
        return qFuzzyIsNull(s)?0.0:std::ceil(v/s)*s;});
    alias("天花板","CEILING"); alias("上取整","CEILING");

    reg("LOG",[](const QVector<QVariant>& a)->QVariant{
        double v=toNum(a.value(0)),b=a.size()>=2?toNum(a.value(1)):10.0;
        return (v>0&&b>0)?std::log(v)/std::log(b):0.0;});
    alias("对数","LOG");

    reg("LOG10",[](const QVector<QVariant>& a)->QVariant{
        double v=toNum(a.value(0)); return v>0?std::log10(v):0.0;});
    alias("常用对数","LOG10");

    reg("LN",[](const QVector<QVariant>& a)->QVariant{
        double v=toNum(a.value(0)); return v>0?std::log(v):0.0;});
    alias("自然对数","LN");

    reg("EXP",[](const QVector<QVariant>& a)->QVariant{
        return std::exp(toNum(a.value(0)));});
    alias("指数","EXP");

    reg("PI",[](const QVector<QVariant>&)->QVariant{ return M_PI;});
    alias("圆周率","PI");

    reg("SIGN",[](const QVector<QVariant>& a)->QVariant{
        double v=toNum(a.value(0)); return v>0?1.0:(v<0?-1.0:0.0);});
    alias("符号","SIGN"); alias("正负号","SIGN");

    reg("GCD",[](const QVector<QVariant>& a)->QVariant{
        auto gcd=[](long long x,long long y)->long long{
            while(y){x%=y;std::swap(x,y);}return std::abs(x);};
        if(a.size()<2) return toNum(a.value(0));
        long long g=(long long)toNum(a[0]);
        for(int i=1;i<a.size();++i) g=gcd(g,(long long)toNum(a[i]));
        return (double)g;});
    alias("最大公约数","GCD");

    reg("LCM",[](const QVector<QVariant>& a)->QVariant{
        auto gcd=[](long long x,long long y)->long long{
            while(y){x%=y;std::swap(x,y);}return std::abs(x);};
        if(a.size()<2) return toNum(a.value(0));
        long long r=(long long)toNum(a[0]);
        for(int i=1;i<a.size();++i){
            long long b=(long long)toNum(a[i]); r=r/gcd(r,b)*b;}
        return (double)r;});
    alias("最小公倍数","LCM");

    reg("RAND",[](const QVector<QVariant>&)->QVariant{
        return (double)std::rand()/RAND_MAX;});
    alias("随机数","RAND");

    // ════ 统计 ════════════════════════════════════
    reg("MEDIAN",[](const QVector<QVariant>& a)->QVariant{
        QVector<double> v; for(const auto& x:a) v<<toNum(x);
        std::sort(v.begin(),v.end());
        int n=v.size(); if(n==0) return 0.0;
        return n%2==0?(v[n/2-1]+v[n/2])/2.0:v[n/2];});
    alias("中位数","MEDIAN"); alias("中值","MEDIAN");

    reg("STDEV",[](const QVector<QVariant>& a)->QVariant{
        if(a.size()<2) return 0.0;
        double s=0,s2=0;
        for(const auto& x:a){double v=toNum(x);s+=v;s2+=v*v;}
        int n=a.size(); double var=(s2-s*s/n)/(n-1);
        return var>=0?std::sqrt(var):0.0;});
    alias("标准差","STDEV"); alias("标准偏差","STDEV");
    reg("STD",registry()[normKey("STDEV")]);

    reg("VAR",[](const QVector<QVariant>& a)->QVariant{
        if(a.size()<2) return 0.0;
        double s=0,s2=0;
        for(const auto& x:a){double v=toNum(x);s+=v;s2+=v*v;}
        int n=a.size(); return (s2-s*s/n)/(n-1);});
    alias("方差","VAR");

    reg("LARGE",[](const QVector<QVariant>& a)->QVariant{
        if(a.size()<2) return 0.0;
        QVector<double> v; for(int i=0;i<a.size()-1;++i) v<<toNum(a[i]);
        std::sort(v.begin(),v.end(),std::greater<double>());
        int k=(int)toNum(a.last())-1;
        return (k>=0&&k<v.size())?v[k]:0.0;});
    alias("第大值","LARGE");

    reg("SMALL",[](const QVector<QVariant>& a)->QVariant{
        if(a.size()<2) return 0.0;
        QVector<double> v; for(int i=0;i<a.size()-1;++i) v<<toNum(a[i]);
        std::sort(v.begin(),v.end());
        int k=(int)toNum(a.last())-1;
        return (k>=0&&k<v.size())?v[k]:0.0;});
    alias("第小值","SMALL");

    // ════ 文本 ════════════════════════════════════
    reg("LEN",[](const QVector<QVariant>& a)->QVariant{
        return (double)a.value(0).toString().length();});
    alias("长度","LEN"); alias("字符数","LEN");
    reg("LENGTH",registry()[normKey("LEN")]);

    reg("LEFT",[](const QVector<QVariant>& a)->QVariant{
        QString s=a.value(0).toString();
        int n=a.size()>=2?(int)toNum(a.value(1)):1;
        return s.left(n);});
    alias("取左","LEFT"); alias("左取","LEFT");

    reg("RIGHT",[](const QVector<QVariant>& a)->QVariant{
        QString s=a.value(0).toString();
        int n=a.size()>=2?(int)toNum(a.value(1)):1;
        return s.right(n);});
    alias("取右","RIGHT"); alias("右取","RIGHT");

    reg("MID",[](const QVector<QVariant>& a)->QVariant{
        QString s=a.value(0).toString();
        int start=a.size()>=2?(int)toNum(a.value(1))-1:0;
        int len=a.size()>=3?(int)toNum(a.value(2)):s.length();
        return s.mid(qMax(0,start),len);});
    alias("取中","MID"); alias("中间","MID");

    reg("UPPER",[](const QVector<QVariant>& a)->QVariant{
        return a.value(0).toString().toUpper();});
    alias("大写","UPPER"); alias("转大写","UPPER");

    reg("LOWER",[](const QVector<QVariant>& a)->QVariant{
        return a.value(0).toString().toLower();});
    alias("小写","LOWER"); alias("转小写","LOWER");

    reg("TRIM",[](const QVector<QVariant>& a)->QVariant{
        return a.value(0).toString().trimmed();});
    alias("去空格","TRIM"); alias("去除空格","TRIM");

    reg("CONCAT",[](const QVector<QVariant>& a)->QVariant{
        QString r; for(const auto& v:a) r+=v.toString(); return r;});
    alias("拼接","CONCAT"); alias("连接","CONCAT"); alias("合并","CONCAT");
    reg("CONCATENATE",registry()[normKey("CONCAT")]);

    reg("REPT",[](const QVector<QVariant>& a)->QVariant{
        return a.value(0).toString().repeated(qMax(0,(int)toNum(a.value(1))));});
    alias("重复","REPT");

    reg("FIND",[](const QVector<QVariant>& a)->QVariant{
        QString needle=a.value(0).toString(),hay=a.value(1).toString();
        int start=a.size()>=3?(int)toNum(a.value(2))-1:0;
        int p=hay.indexOf(needle,start);
        return p<0?0.0:(double)(p+1);});
    alias("查找","FIND");

    reg("SUBSTITUTE",[](const QVector<QVariant>& a)->QVariant{
        return a.value(0).toString()
                .replace(a.value(1).toString(),a.value(2).toString());});
    alias("替换","SUBSTITUTE");
    reg("REPLACE",registry()[normKey("SUBSTITUTE")]);

    reg("VALUE",[](const QVector<QVariant>& a)->QVariant{
        return toNum(a.value(0));});
    alias("转数字","VALUE"); alias("数值","VALUE");

    reg("TEXT",[](const QVector<QVariant>& a)->QVariant{
        double v=toNum(a.value(0));
        QString fmt=a.value(1).toString();
        int dec=0; int dot=fmt.indexOf('.');
        if(dot>=0) dec=fmt.length()-dot-1;
        return QString::number(v,'f',dec);});
    alias("文本","TEXT"); alias("格式化","TEXT");

    reg("EXACT",[](const QVector<QVariant>& a)->QVariant{
        return a.value(0).toString()==a.value(1).toString()?1.0:0.0;});
    alias("精确比较","EXACT");

    // ════ 逻辑 ════════════════════════════════════
    reg("AND",[](const QVector<QVariant>& a)->QVariant{
        for(const auto& v:a) if(toNum(v)==0.0) return 0.0; return 1.0;});
    alias("且","AND"); alias("并且","AND"); alias("与","AND");

    reg("OR",[](const QVector<QVariant>& a)->QVariant{
        for(const auto& v:a) if(toNum(v)!=0.0) return 1.0; return 0.0;});
    alias("或","OR"); alias("或者","OR");

    reg("NOT",[](const QVector<QVariant>& a)->QVariant{
        return toNum(a.value(0))==0.0?1.0:0.0;});
    alias("非","NOT"); alias("取反","NOT"); alias("否","NOT");

    reg("XOR",[](const QVector<QVariant>& a)->QVariant{
        int t=0; for(const auto& v:a) if(toNum(v)!=0.0) t++;
        return t%2==1?1.0:0.0;});
    alias("异或","XOR");

    reg("ISNUMBER",[](const QVector<QVariant>& a)->QVariant{
        bool ok; a.value(0).toString().toDouble(&ok); return ok?1.0:0.0;});
    alias("是数字","ISNUMBER");

    reg("ISBLANK",[](const QVector<QVariant>& a)->QVariant{
        return a.value(0).toString().trimmed().isEmpty()?1.0:0.0;});
    alias("是否为空","ISBLANK"); alias("为空","ISBLANK");
    reg("ISEMPTY",registry()[normKey("ISBLANK")]);

    reg("ISTEXT",[](const QVector<QVariant>& a)->QVariant{
        bool ok; a.value(0).toString().toDouble(&ok); return ok?0.0:1.0;});
    alias("是文本","ISTEXT");

    reg("CHOOSE",[](const QVector<QVariant>& a)->QVariant{
        int i=(int)toNum(a.value(0));
        return (i>=1&&i<a.size())?a[i]:QVariant();});
    alias("选择","CHOOSE");

    // ════ 日期 ════════════════════════════════════
    reg("TODAY",[](const QVector<QVariant>&)->QVariant{
        return QDate::currentDate().toString("yyyy-MM-dd");});
    alias("今天","TODAY");

    reg("NOW",[](const QVector<QVariant>&)->QVariant{
        return QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");});
    alias("现在","NOW");

    reg("YEAR",[](const QVector<QVariant>& a)->QVariant{
        QDate d=QDate::fromString(a.value(0).toString(),"yyyy-MM-dd");
        return (double)(d.isValid()?d.year():QDate::currentDate().year());});
    alias("年","YEAR"); alias("年份","YEAR");

    reg("MONTH",[](const QVector<QVariant>& a)->QVariant{
        QDate d=QDate::fromString(a.value(0).toString(),"yyyy-MM-dd");
        return (double)(d.isValid()?d.month():QDate::currentDate().month());});
    alias("月","MONTH"); alias("月份","MONTH");

    reg("DAY",[](const QVector<QVariant>& a)->QVariant{
        QDate d=QDate::fromString(a.value(0).toString(),"yyyy-MM-dd");
        return (double)(d.isValid()?d.day():QDate::currentDate().day());});
    alias("日","DAY"); alias("天","DAY");

    reg("DAYS",[](const QVector<QVariant>& a)->QVariant{
        QDate d1=QDate::fromString(a.value(0).toString(),"yyyy-MM-dd");
        QDate d2=QDate::fromString(a.value(1).toString(),"yyyy-MM-dd");
        return (d1.isValid()&&d2.isValid())?(double)d2.daysTo(d1):0.0;});
    alias("相差天数","DAYS");

    reg("DATE",[](const QVector<QVariant>& a)->QVariant{
        int y=(int)toNum(a.value(0)),m=(int)toNum(a.value(1)),d=(int)toNum(a.value(2));
        return QDate(y,m,d).toString("yyyy-MM-dd");});
    alias("日期","DATE");

    reg("WEEKDAY",[](const QVector<QVariant>& a)->QVariant{
        QDate d=QDate::fromString(a.value(0).toString(),"yyyy-MM-dd");
        return d.isValid()?(double)d.dayOfWeek():0.0;});
    alias("星期","WEEKDAY"); alias("周几","WEEKDAY");
}

// ══════════════════════════════════════════════
//  词法分析器
// ══════════════════════════════════════════════
ExprEvaluator::Lexer::Lexer(const QString& s, const QStringList& c)
    : src(s), pos(0) {
    cols = c;
    std::sort(cols.begin(), cols.end(),
              [](const QString& a, const QString& b){ return a.size()>b.size(); });
}

void ExprEvaluator::Lexer::skipWS() {
    while (pos < src.size() && src[pos].isSpace()) ++pos;
}

ExprEvaluator::Tok ExprEvaluator::Lexer::read() {
    skipWS();
    if (pos >= src.size()) return {TT::End};

    // 带引号的字符串
    if (src[pos]=='"'||src[pos]=='\'') {
        QChar q=src[pos++]; int start=pos;
        while (pos<src.size()&&src[pos]!=q) ++pos;
        QString s=src.mid(start,pos-start);
        if (pos<src.size()) ++pos;
        return {TT::Str,0,s};
    }

    // 特殊关键字（惰性求值形式）
    struct KW { QString name; TT tt; };
    static const KW keywords[] = {
        {"IFS",  TT::IFS}, {"多条件",TT::IFS}, {"条件组",TT::IFS},
        {"IF",   TT::IF},  {"如果",  TT::IF},
        {"",     TT::End}
    };
    for (const auto& kw : keywords) {
        if (kw.name.isEmpty()) break;
        int len=kw.name.length();
        if (src.mid(pos,len)==kw.name||src.mid(pos,len).toUpper()==kw.name.toUpper()) {
            int after=pos+len;
            if (after>=src.size()||(!src[after].isLetterOrNumber()&&src[after]!='_')) {
                pos+=len; return {kw.tt};
            }
        }
    }

    // 已知列名（优先，长度降序）
    for (const auto& col : cols) {
        int len=col.length();
        if (src.mid(pos,len)==col) {
            int after=pos+len;
            if (after>=src.size()||
                (!src[after].isLetterOrNumber()&&src[after]!='_'&&src[after]!='(')) {
                pos+=len; return {TT::Col,0,col};
            }
            if (src[after]=='(') { pos+=len; return {TT::Col,0,col}; }
        }
    }

    // 数字
    if (src[pos].isDigit()||(src[pos]=='.'&&pos+1<src.size()&&src[pos+1].isDigit())) {
        int start=pos;
        while (pos<src.size()&&(src[pos].isDigit()||src[pos]=='.')) ++pos;
        return {TT::Num,src.mid(start,pos-start).toDouble()};
    }

    // 运算符
    QChar c=src[pos++];
    if (c=='+') return {TT::Plus};
    if (c=='-') return {TT::Minus};
    if (c=='*') return {TT::Mul};
    if (c=='/') return {TT::Div};
    if (c=='(') return {TT::LParen};
    if (c==')') return {TT::RParen};
    if (c==',') return {TT::Comma};
    if (c=='>'&&pos<src.size()&&src[pos]=='='){++pos;return{TT::GE};}
    if (c=='<'&&pos<src.size()&&src[pos]=='='){++pos;return{TT::LE};}
    if (c=='>') return {TT::GT};
    if (c=='<') return {TT::LT};
    if (c=='='&&pos<src.size()&&src[pos]=='='){++pos;return{TT::EQ};}
    if (c=='!'&&pos<src.size()&&src[pos]=='='){++pos;return{TT::NE};}

    // 无引号文本标签 / 函数名
    if (c.unicode()>127||c.isLetter()) {
        --pos; int start=pos;
        while (pos<src.size()) {
            QChar ch=src[pos];
            if (ch==','||ch==')'||ch=='('||ch=='"'||ch=='\'') break;
            if (ch.isSpace()) break;
            ++pos;
        }
        QString label=src.mid(start,pos-start).trimmed();
        if (!label.isEmpty()) return {TT::Col,0,label};
    }

    return {TT::Err,0,QString("未知符号: ")+c};
}

ExprEvaluator::Tok ExprEvaluator::Lexer::next() {
    if (!buf.isEmpty()){auto t=buf.first();buf.removeFirst();return t;}
    return read();
}
ExprEvaluator::Tok ExprEvaluator::Lexer::peek() {
    if (buf.isEmpty()) buf<<read();
    return buf.first();
}
bool ExprEvaluator::Lexer::atEnd() {
    return peek().t==TT::End;
}

// ══════════════════════════════════════════════
//  语法分析器
// ══════════════════════════════════════════════

// expr = addExpr (compOp addExpr)?
ExprEvaluator::Parser::Fn ExprEvaluator::Parser::parseExpr() {
    auto left=parseAdd();
    if (!error.isEmpty()) return left;
    TT tt=lex.peek().t;
    if (tt==TT::GE||tt==TT::LE||tt==TT::GT||tt==TT::LT||tt==TT::EQ||tt==TT::NE) {
        auto op=lex.next().t;
        auto right=parseAdd();
        auto L=left,R=right;
        return [L,R,op](const Row& r)->QVariant{
            double lv=L(r).toDouble(),rv=R(r).toDouble(); bool c=false;
            switch(op){
                case TT::GE:c=lv>=rv;break; case TT::LE:c=lv<=rv;break;
                case TT::GT:c=lv>rv; break; case TT::LT:c=lv<rv; break;
                case TT::EQ:c=qFuzzyCompare(lv+1,rv+1);break;
                case TT::NE:c=!qFuzzyCompare(lv+1,rv+1);break;
                default:break;
            }
            return c?1.0:0.0;
        };
    }
    return left;
}

// addExpr = mulExpr (('+' | '-') mulExpr)*
ExprEvaluator::Parser::Fn ExprEvaluator::Parser::parseAdd() {
    auto left=parseMul();
    if (!error.isEmpty()) return left;
    while (lex.peek().t==TT::Plus||lex.peek().t==TT::Minus) {
        auto op=lex.next().t;
        auto right=parseMul();
        if (!error.isEmpty()) return left;
        auto L=left,R=right;
        if (op==TT::Plus)
            left=[L,R](const Row& r)->QVariant{return L(r).toDouble()+R(r).toDouble();};
        else
            left=[L,R](const Row& r)->QVariant{return L(r).toDouble()-R(r).toDouble();};
    }
    return left;
}

// mulExpr = unary (('*' | '/') unary)*
ExprEvaluator::Parser::Fn ExprEvaluator::Parser::parseMul() {
    auto left=parseUnary();
    if (!error.isEmpty()) return left;
    while (lex.peek().t==TT::Mul||lex.peek().t==TT::Div) {
        auto op=lex.next().t;
        auto right=parseUnary();
        if (!error.isEmpty()) return left;
        auto L=left,R=right;
        if (op==TT::Mul)
            left=[L,R](const Row& r)->QVariant{return L(r).toDouble()*R(r).toDouble();};
        else
            left=[L,R](const Row& r)->QVariant{
                double d=R(r).toDouble();
                return qFuzzyIsNull(d)?0.0:L(r).toDouble()/d;};
    }
    return left;
}

// unary = '-' primary | primary
ExprEvaluator::Parser::Fn ExprEvaluator::Parser::parseUnary() {
    if (lex.peek().t==TT::Minus) {
        lex.next();
        auto inner=parsePrimary();
        return [inner](const Row& r)->QVariant{return -inner(r).toDouble();};
    }
    return parsePrimary();
}

// 通用函数调用（registry）
ExprEvaluator::Parser::Fn ExprEvaluator::Parser::parseFuncCall(const QString& name) {
    lex.next(); // consume '('
    QVector<Fn> argFns;
    if (lex.peek().t!=TT::RParen) {
        argFns<<parseExpr();
        while (lex.peek().t==TT::Comma) {
            lex.next();
            argFns<<parseExpr();
        }
    }
    if (lex.next().t!=TT::RParen) {
        error="函数 "+name+" 缺少 ')'"; return {};
    }
    QString key=ExprEvaluator::normKey(name);
    auto& reg=ExprEvaluator::registry();
    if (!reg.contains(key)) {
        error="未知函数: "+name; return {};
    }
    auto fn=reg[key];
    return [fn,argFns](const Row& r)->QVariant{
        QVector<QVariant> args;
        for(const auto& af:argFns) args<<af(r);
        return fn(args);
    };
}

// primary
ExprEvaluator::Parser::Fn ExprEvaluator::Parser::parsePrimary() {
    Tok t=lex.next();

    if (t.t==TT::Num) {
        double v=t.num;
        return [v](const Row&)->QVariant{return v;};
    }
    if (t.t==TT::Str) {
        QString s=t.s;
        return [s](const Row&)->QVariant{return s;};
    }
    if (t.t==TT::Col) {
        QString name=t.s;
        if (lex.peek().t==TT::LParen) return parseFuncCall(name);
        return [name](const Row& r)->QVariant{
            if (r.contains(name)) {
                bool ok; double d=r[name].toString().toDouble(&ok);
                return ok?QVariant(d):r[name];
            }
            return name; // 无引号标签
        };
    }
    if (t.t==TT::LParen) {
        auto inner=parseExpr();
        if (lex.peek().t==TT::RParen) lex.next();
        else error="缺少右括号 ')'";
        return inner;
    }
    if (t.t==TT::IF)  return parseIF();
    if (t.t==TT::IFS) return parseIFS();
    if (t.t==TT::End) error="表达式意外结束";
    else               error="无法识别: "+t.s;
    return [](const Row&)->QVariant{return 0.0;};
}

// IF(cond, trueExpr, falseExpr)
ExprEvaluator::Parser::Fn ExprEvaluator::Parser::parseIF() {
    if (lex.next().t!=TT::LParen){error="IF 后需要 '('";return {};}
    auto cond=parseExpr();
    if (lex.next().t!=TT::Comma){error="IF 条件后需要 ','";return {};}
    auto trueE=parseExpr();
    if (lex.next().t!=TT::Comma){error="IF 真值后需要 ','";return {};}
    auto falseE=parseExpr();
    if (lex.next().t!=TT::RParen){error="IF 缺少 ')'";return {};}
    return [cond,trueE,falseE](const Row& r)->QVariant{
        return cond(r).toDouble()!=0.0?trueE(r):falseE(r);
    };
}

// IFS(cond1,val1, cond2,val2, ..., [default])
// 支持奇数（含默认值）和偶数（无默认值）参数
ExprEvaluator::Parser::Fn ExprEvaluator::Parser::parseIFS() {
    if (lex.next().t!=TT::LParen){error="IFS 后需要 '('";return {};}
    QVector<Fn> args;
    args<<parseExpr();
    while (lex.peek().t==TT::Comma){lex.next();args<<parseExpr();}
    if (lex.next().t!=TT::RParen){error="IFS 缺少 ')'";return {};}
    if (args.size()<2){
        error="IFS 至少需要一对参数";return {};}

    bool hasDefault=(args.size()%2==1); // 奇数=有默认值，偶数=全是条件对
    return [args,hasDefault](const Row& r)->QVariant{
        int pairs=hasDefault?(args.size()-1)/2:args.size()/2;
        for(int i=0;i<pairs*2;i+=2)
            if(args[i](r).toDouble()!=0.0) return args[i+1](r);
        return hasDefault?args.last()(r):QVariant(QString("—"));
    };
}

// ══════════════════════════════════════════════
//  编译入口
// ══════════════════════════════════════════════
ExprEvaluator::Compiled ExprEvaluator::compile(const QString& expr,
                                                 const QStringList& knownCols) {
    initRegistry();
    Compiled c;
    Lexer lex(expr.trimmed(), knownCols);
    Parser parser{lex, ""};
    auto fn=parser.parseExpr();
    if (!parser.error.isEmpty()){c.error=parser.error;return c;}
    c.isNumeric=!expr.contains('"')&&!expr.contains('\'');
    c.eval=fn;
    return c;
}