#pragma once
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QMap>
#include <functional>

class ExprEvaluator {
public:
    using Row = QMap<QString, QVariant>;

    struct Compiled {
        std::function<QVariant(const Row&)> eval;
        bool    isNumeric = true;
        QString error;
        bool ok() const { return error.isEmpty(); }
    };

    static Compiled compile(const QString& expr, const QStringList& knownCols);

    using EagerFn = std::function<QVariant(QVector<QVariant>)>;
    static void registerFunction(const QString& name, EagerFn fn);
    static QString normKey(const QString& name);

private:
    enum class TT {
        Num, Str, Col,
        Plus, Minus, Mul, Div,
        LParen, RParen, Comma,
        GE, LE, GT, LT, EQ, NE,
        IF, IFS,
        End, Err
    };
    struct Tok { TT t; double num=0; QString s; };

    struct Lexer {
        QString      src;
        int          pos = 0;
        QStringList  cols;
        QVector<Tok> buf;  // public in struct so Parser can access

        Lexer(const QString& s, const QStringList& c);
        Tok  next();
        Tok  peek();
        bool atEnd();
    private:
        Tok  read();
        void skipWS();
    };

    struct Parser {
        Lexer&  lex;
        QString error;
        using Fn = std::function<QVariant(const Row&)>;

        Fn parseExpr();
        Fn parseAdd();
        Fn parseMul();
        Fn parseUnary();
        Fn parsePrimary();
        Fn parseIF();
        Fn parseIFS();
        Fn parseFuncCall(const QString& name);
    };

    static QMap<QString, EagerFn>& registry();
    static void                    initRegistry();
};