#include "ApiSettingsDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

static QString normalizeUrl(const QString& url, ApiFormat fmt) {
    QString u = url.trimmed();
    if (u.endsWith("/")) u.chop(1);
    if (fmt == ApiFormat::OpenAI) {
        if (!u.endsWith("/chat/completions") && !u.endsWith("/completions"))
            u += "/chat/completions";
    } else {
        if (!u.endsWith("/messages"))
            u += (u.endsWith("/v1") ? "/messages" : "/v1/messages");
    }
    return u;
}

ApiSettingsDialog::ApiSettingsDialog(const ApiConfig& cur, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("AI API 设置");
    setMinimumWidth(540);

    auto* lay = new QVBoxLayout(this);

    // ── 配置文件路径提示 ──────────────────────────
    auto* pathTip = new QLabel(
        "🔒 API Key 保存在 <b>C:/ExcelProcessorConfig/api_config.json</b>",
        this);
    pathTip->setWordWrap(true);
    pathTip->setStyleSheet(
        "background:#1a3a1a;color:#66ff88;padding:6px 10px;border-radius:4px;font-size:11px;");
    lay->addWidget(pathTip);

    // ── 预设选择 ──────────────────────────────────
    auto* presetGrp = new QGroupBox("快速选择预设", this);
    auto* presetLay = new QHBoxLayout(presetGrp);

    m_preset = new QComboBox(this);
    m_preset->addItem("Hermes（本地/自托管）",  0);
    m_preset->addItem("Anthropic Claude",       1);
    m_preset->addItem("OpenAI GPT",             2);
    m_preset->addItem("DeepSeek",               3);
    m_preset->addItem("阿里通义千问",            4);
    m_preset->addItem("Ollama（本地）",          5);
    m_preset->addItem("自定义",                 99);

    presetLay->addWidget(new QLabel("选择服务商："));
    presetLay->addWidget(m_preset, 1);
    lay->addWidget(presetGrp);

    // ── 详细配置 ──────────────────────────────────
    auto* cfgGrp = new QGroupBox("详细配置", this);
    auto* fl     = new QFormLayout(cfgGrp);

    m_format = new QComboBox(this);
    m_format->addItem("OpenAI 兼容格式（Hermes / 本地 / 大多数服务）",
                       (int)ApiFormat::OpenAI);
    m_format->addItem("Anthropic 格式（仅官方 Claude API）",
                       (int)ApiFormat::Anthropic);

    m_url   = new QLineEdit(cur.url,    this);
    m_key   = new QLineEdit(cur.apiKey, this);
    m_model = new QLineEdit(cur.model,  this);

    m_key->setEchoMode(QLineEdit::Password);
    m_key->setPlaceholderText("输入你的 API Key");
    m_url->setPlaceholderText("http://localhost:端口/v1/chat/completions");
    m_model->setPlaceholderText("模型名称，如 hermes-3-llama-3.1-8b");

    fl->addRow("API 格式：", m_format);
    fl->addRow("API 地址：", m_url);
    fl->addRow("API Key：",  m_key);
    fl->addRow("模型名称：", m_model);
    lay->addWidget(cfgGrp);

    // ── 常用地址参考 ──────────────────────────────
    auto* tip = new QLabel(
        "<b>常用地址参考：</b><br>"
        "Hermes（LM Studio）：http://localhost:1234/v1/chat/completions<br>"
        "Hermes（Ollama）：http://localhost:11434/v1/chat/completions<br>"
        "Anthropic：https://api.anthropic.com/v1/messages<br>"
        "DeepSeek：https://api.deepseek.com/v1/chat/completions<br>"
        "通义千问：https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions",
        this);
    tip->setStyleSheet("color:#777;font-size:11px;padding:4px;");
    tip->setWordWrap(true);
    lay->addWidget(tip);

    // ── 测试连接 ──────────────────────────────────
    m_testStatus = new QLabel("", this);
    m_testStatus->setWordWrap(true);

    auto* testRow = new QHBoxLayout;
    auto* btnTest = new QPushButton("🔗 测试连接", this);
    connect(btnTest, &QPushButton::clicked, this, &ApiSettingsDialog::onTestApi);
    testRow->addWidget(btnTest);
    testRow->addWidget(m_testStatus, 1);
    lay->addLayout(testRow);

    // ── 确定/取消 ─────────────────────────────────
    auto* box = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    lay->addWidget(box);

    // 初始化格式
    int fi = m_format->findData((int)cur.format);
    if (fi >= 0) m_format->setCurrentIndex(fi);

    connect(m_preset, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ApiSettingsDialog::onPresetChanged);
}

// ── 预设切换 ───────────────────────────────────
void ApiSettingsDialog::onPresetChanged(int) {
    int id = m_preset->currentData().toInt();
    ApiConfig cfg;
    switch (id) {
        case 0: // Hermes
            cfg.url    = "http://localhost:1234/v1/chat/completions";
            cfg.model  = "hermes-3-llama-3.1-8b";
            cfg.format = ApiFormat::OpenAI;
            break;
        case 1: cfg = ApiConfig::anthropic(); break;
        case 2: cfg = ApiConfig::openai();    break;
        case 3: cfg = ApiConfig::deepseek();  break;
        case 4: cfg = ApiConfig::tongyi();    break;
        case 5: cfg = ApiConfig::ollama();    break;
        default: return;
    }
    m_url->setText(cfg.url);
    m_model->setText(cfg.model);
    m_format->setCurrentIndex(m_format->findData((int)cfg.format));
}

// ── 测试连接 ───────────────────────────────────
void ApiSettingsDialog::onTestApi() {
    m_testStatus->setText("⏳ 测试中...");
    m_testStatus->setStyleSheet("color:#888;");

    ApiConfig cfg = result();
    if (cfg.url.trimmed().isEmpty()) {
        m_testStatus->setText("❌ 请填写 API 地址");
        m_testStatus->setStyleSheet("color:#ff4444;");
        return;
    }

    auto* net = new QNetworkAccessManager(this);
    QNetworkRequest req(QUrl(normalizeUrl(cfg.url, cfg.format)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject body;
    QJsonArray msgs;
    QJsonObject msg; msg["role"]="user"; msg["content"]="hi"; msgs.append(msg);

    if (cfg.format == ApiFormat::Anthropic) {
        req.setRawHeader("x-api-key",        cfg.apiKey.toUtf8());
        req.setRawHeader("anthropic-version", "2023-06-01");
        body["model"]="claude-haiku-4-5-20251001"; body["max_tokens"]=5;
        body["messages"]=msgs;
    } else {
        req.setRawHeader("Authorization", ("Bearer " + cfg.apiKey).toUtf8());
        body["model"]=cfg.model; body["max_tokens"]=5; body["messages"]=msgs;
    }

    auto* reply = net->post(req, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            m_testStatus->setText("✅ 连接成功！");
            m_testStatus->setStyleSheet("color:#00cc66;font-weight:bold;");
        } else {
            QString body = reply->readAll().left(150);
            m_testStatus->setText("❌ " + reply->errorString()
                + (body.isEmpty() ? "" : "\n" + body));
            m_testStatus->setStyleSheet("color:#ff4444;");
        }
    });
}

ApiConfig ApiSettingsDialog::result() const {
    return {
        m_url->text().trimmed(),
        m_key->text().trimmed(),
        m_model->text().trimmed(),
        (ApiFormat)m_format->currentData().toInt()
    };
}