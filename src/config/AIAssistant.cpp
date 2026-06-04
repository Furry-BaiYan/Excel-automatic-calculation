#include "AIAssistant.h"
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QDebug>

// ── 配置文件路径（C盘，项目目录外，不会被 git 提交）──
static QString configPath() {
    return "C:/ExcelProcessorConfig/api_config.json";
}

// ── 自动补全 API 路径 ──────────────────────────  ← 加在这里
static QString normalizeUrl(const QString& url, ApiFormat fmt) {
    QString u = url.trimmed();
    if (u.endsWith("/")) u.chop(1);

    if (fmt == ApiFormat::OpenAI) {
        // 填了 /v1 → 补成 /v1/chat/completions
        if (!u.endsWith("/chat/completions") && !u.endsWith("/completions"))
            u += "/chat/completions";
    } else {
        // Anthropic 填了基础地址 → 补成 /v1/messages
        if (!u.endsWith("/messages"))
            u += (u.endsWith("/v1") ? "/messages" : "/v1/messages");
    }
    return u;
}

AIAssistant::AIAssistant(QObject* parent)
    : QObject(parent), m_net(new QNetworkAccessManager(this)) {
    connect(m_net, &QNetworkAccessManager::finished,
            this, &AIAssistant::onReply);
    loadConfig();
}

// ── 保存配置到 C 盘文件 ────────────────────────
void AIAssistant::saveConfig() {
    QString path = configPath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QJsonObject obj;
    obj["url"]    = m_cfg.url;
    obj["apiKey"] = m_cfg.apiKey;
    obj["model"]  = m_cfg.model;
    obj["format"] = (int)m_cfg.format;

    QFile f(path);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
        qDebug() << "[AI] 配置已保存到:" << path;
    } else {
        qWarning() << "[AI] 无法写入配置文件:" << path;
    }
}

// ── 从 C 盘文件加载配置 ────────────────────────
void AIAssistant::loadConfig() {
    QFile f(configPath());
    if (!f.open(QIODevice::ReadOnly)) {
        qDebug() << "[AI] 配置文件不存在，使用默认值:" << configPath();
        return;
    }
    QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
    if (obj.contains("url"))    m_cfg.url    = obj["url"].toString();
    if (obj.contains("apiKey")) m_cfg.apiKey = obj["apiKey"].toString();
    if (obj.contains("model"))  m_cfg.model  = obj["model"].toString();
    if (obj.contains("format")) m_cfg.format = (ApiFormat)obj["format"].toInt();
    qDebug() << "[AI] 配置已从 C 盘加载，模型:" << m_cfg.model;
}

void AIAssistant::setConfig(const ApiConfig& cfg) {
    m_cfg = cfg;
    saveConfig();
}

// ── 构建 Prompt ────────────────────────────────
QString AIAssistant::buildPrompt(const QStringList& cols,
                                  const QVector<QStringList>& rows,
                                  const QString& formatReq) const {
    QString p;
    p += "你是一个数据分析专家，帮助用户为Excel数据设计计算公式。\n\n";
    p += "数据列名：" + cols.join("、") + "\n\n";
    if (!rows.isEmpty()) {
        p += "样本数据（前几行）：\n";
        p += cols.join("\t") + "\n";
        for (const auto& row : rows) p += row.join("\t") + "\n";
        p += "\n";
    }

    // ── 格式要求（有就严格遵守，没有就自动判断）──
    if (!formatReq.trimmed().isEmpty()) {
        p += "用户格式要求（必须严格遵守）：\n";
        p += formatReq.trimmed() + "\n\n";
    }

   p += R"(
请分析列名含义，推荐最有用的计算公式。
直接输出公式，每行一条，格式：新列名 = 表达式
用 # 写注释说明用途。

可用函数（中英文均可）：
数学：SUM/求和, AVERAGE/平均值, MAX/最大值, MIN/最小值,
      ROUND/四舍五入, ABS/绝对值, SQRT/平方根, MOD/取余,
      POWER/幂, INT/取整, ROUNDUP/向上取整, ROUNDDOWN/向下取整
文本：LEN/长度, LEFT/取左, RIGHT/取右, MID/取中, CONCAT/拼接
条件：IF(条件, 真值, 假值)
      IFS/多条件(条件1, 值1, 条件2, 值2, ..., 默认值)
逻辑：AND/且, OR/或, NOT/非

要求：
1. 列名用原始列名，字符串用双引号
2. 推荐 3~6 条最有价值的公式
3. 如果有数值列，给出汇总公式
4. 如果适合分类，给出 IFS 条件分级
5. 只输出公式，不要额外解释
6. 公式全部写完后，另起一行输出统计配置（格式固定不变）：

## 统计配置
统计列: 列名1, 列名2, ...
分组列: 列名（没有合适的分组列则写 无）
## 报告标题
根据数据内容写一个简短的中文报告标题（不超过15字，不要带"报告"两字）
)";
    return p;
}

// ── 构建请求体 ─────────────────────────────────
QByteArray AIAssistant::buildRequestBody(const QString& prompt) const {
    QJsonObject body;
    QJsonArray msgs;
    QJsonObject userMsg;
    userMsg["role"]    = "user";
    userMsg["content"] = prompt;

    if (m_cfg.format == ApiFormat::Anthropic) {
        body["model"]      = m_cfg.model;
        body["max_tokens"] = 1024;
        msgs.append(userMsg);
        body["messages"] = msgs;
    } else {
        // OpenAI 兼容格式
        QJsonObject sysMsg;
        sysMsg["role"]    = "system";
        sysMsg["content"] = "你是一个Excel数据分析专家，只输出公式代码，不要额外解释。";
        msgs.append(sysMsg);
        msgs.append(userMsg);
        body["model"]       = m_cfg.model;
        body["max_tokens"]  = 1024;
        body["temperature"] = 0.3;
        body["messages"]    = msgs;
    }
    return QJsonDocument(body).toJson(QJsonDocument::Compact);
}

// ── 设置请求头 ─────────────────────────────────
void AIAssistant::setRequestHeaders(QNetworkRequest& req) const {
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (m_cfg.format == ApiFormat::Anthropic) {
        req.setRawHeader("x-api-key",        m_cfg.apiKey.toUtf8());
        req.setRawHeader("anthropic-version", "2023-06-01");
    } else {
        req.setRawHeader("Authorization",
            ("Bearer " + m_cfg.apiKey).toUtf8());
    }
}

// ── 发送请求 ───────────────────────────────────
void AIAssistant::analyze(const QStringList& columns,
                           const QVector<QStringList>& sampleRows,
                           const QString& formatReq) {
    if (m_cfg.apiKey.trimmed().isEmpty()) {
        emit errorOccurred("请先设置 API Key（点击⚙️）");
        return;
    }
    QNetworkRequest req(QUrl(normalizeUrl(m_cfg.url, m_cfg.format)));
    setRequestHeaders(req);
    m_net->post(req, buildRequestBody(buildPrompt(columns, sampleRows)));
    emit analyzing();
    qDebug() << "[AI] 请求 →" << m_cfg.url << "| 模型:" << m_cfg.model;
}

// ── 处理响应 ───────────────────────────────────
void AIAssistant::onReply(QNetworkReply* reply) {
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        emit errorOccurred("请求失败：" + reply->errorString()
            + "\n" + reply->readAll().left(200));
        return;
    }
    QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
    if (obj.contains("error")) {
        emit errorOccurred("API 错误：" +
            obj["error"].toObject()["message"].toString());
        return;
    }
    QString text;
    if (m_cfg.format == ApiFormat::Anthropic) {
        for (const auto& b : obj["content"].toArray())
            if (b.toObject()["type"].toString() == "text")
                text += b.toObject()["text"].toString();
    } else {
        auto choices = obj["choices"].toArray();
        if (!choices.isEmpty())
            text = choices[0].toObject()["message"]
                            .toObject()["content"].toString();
    }
    if (text.trimmed().isEmpty()) { emit errorOccurred("AI 返回内容为空"); return; }
    emit suggestionsReady(text.trimmed());
}

void AIAssistant::fixFormulas(const QString& formulas,
                               const QStringList& errors,
                               const QStringList& columns) {
    if (m_cfg.apiKey.trimmed().isEmpty()) {
        emit errorOccurred("请先设置 API Key");
        return;
    }

    QString prompt;
    prompt += "你是一个表达式解析专家，帮助修复公式错误。\n\n";
    prompt += "可用列名：" + columns.join("、") + "\n\n";
    prompt += "当前公式（有错误）：\n" + formulas + "\n\n";
    prompt += "错误信息：\n";
    for (const auto& e : errors) prompt += "- " + e + "\n";
    prompt += R"(
请修复以上公式中的错误，规则：
1. 只输出修复后的公式，每行一条
2. 保持原有逻辑不变，只修语法错误
3. 常见错误：括号不匹配、IFS参数数量错误、列名有空格需用原名
4. 不要输出解释，只输出公式
)";

    QNetworkRequest req(QUrl(normalizeUrl(m_cfg.url, m_cfg.format)));
    setRequestHeaders(req);

    QJsonObject body;
    QJsonArray msgs;
    QJsonObject userMsg; userMsg["role"]="user"; userMsg["content"]=prompt;

    if (m_cfg.format == ApiFormat::Anthropic) {
        body["model"]=m_cfg.model; body["max_tokens"]=1024;
        msgs.append(userMsg); body["messages"]=msgs;
    } else {
        QJsonObject sys; sys["role"]="system";
        sys["content"]="只输出修复后的公式，不要任何解释。";
        msgs.append(sys); msgs.append(userMsg);
        body["model"]=m_cfg.model; body["max_tokens"]=1024;
        body["temperature"]=0.1; body["messages"]=msgs;
    }

    m_net->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    emit analyzing();
}