#pragma once
#include <QObject>
#include <QStringList>
#include <QVector>
#include <QNetworkAccessManager>
#include <QNetworkReply>

// API 格式类型
enum class ApiFormat {
    Anthropic,    // Anthropic Claude 格式
    OpenAI        // OpenAI / 兼容格式（Ollama、DeepSeek、通义等）
};

// API 配置
struct ApiConfig {
    QString   url       = "https://api.anthropic.com/v1/messages";
    QString   apiKey;
    QString   model     = "claude-sonnet-4-20250514";
    ApiFormat format    = ApiFormat::Anthropic;

    // 常用预设
    static ApiConfig anthropic() {
        return {"https://api.anthropic.com/v1/messages",
                "", "claude-sonnet-4-20250514", ApiFormat::Anthropic};
    }
    static ApiConfig openai() {
        return {"https://api.openai.com/v1/chat/completions",
                "", "gpt-4o", ApiFormat::OpenAI};
    }
    static ApiConfig deepseek() {
        return {"https://api.deepseek.com/v1/chat/completions",
                "", "deepseek-chat", ApiFormat::OpenAI};
    }
    static ApiConfig ollama() {
        return {"http://localhost:11434/v1/chat/completions",
                "ollama", "llama3", ApiFormat::OpenAI};
    }
    static ApiConfig tongyi() {
        return {"https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions",
                "", "qwen-turbo", ApiFormat::OpenAI};
    }
};

class AIAssistant : public QObject {
    Q_OBJECT
public:
    explicit AIAssistant(QObject* parent = nullptr);

    void        setConfig(const ApiConfig& cfg);
    ApiConfig   config() const { return m_cfg; }
    void        saveConfig();
    void        loadConfig();

    void analyze(const QStringList& columns,
                 const QVector<QStringList>& sampleRows,
                 const QString& formatReq = "");

    void fixFormulas(const QString& formulas,
                 const QStringList& errors,
                 const QStringList& columns);

signals:
    void suggestionsReady(const QString& formulaText);
    void analyzing();
    void errorOccurred(const QString& msg);

private slots:
    void onReply(QNetworkReply* reply);

private:
    QString buildPrompt(const QStringList& cols,
                        const QVector<QStringList>& rows,
                        const QString& formatReq = "") const;
    QByteArray buildRequestBody(const QString& prompt) const;
    void       setRequestHeaders(QNetworkRequest& req) const;

    QNetworkAccessManager* m_net;
    ApiConfig              m_cfg;
};