#pragma once
#include <QDialog>
#include <QStringList>
#include <QPair>

class QTextEdit;
class QLineEdit;
class QListWidget;
class QComboBox;
class QLabel;
class QPushButton;
class AIAssistant;

class RuleEditor : public QDialog {
    Q_OBJECT
public:
    explicit RuleEditor(const QStringList& columns, QWidget* parent = nullptr);

    void setSampleData(const QVector<QStringList>& rows);

    void    setFormulaText(const QString& text);
    QString getFormulaText() const;

    void    setReportTitle(const QString& t);
    QString getReportTitle() const;

    void setStatConfig(const QString& groupCol, const QStringList& statCols);
    QPair<QString, QStringList> getStatConfig() const;

private slots:
    void onValidate();
    void onSave();
    void onLoad();
    void onAIAnalyze();
    void onSetApiKey();
    void onAISuggestions(const QString& text);
    void onAIError(const QString& msg);
    void onAIAnalyzing();
    void onAIFix();
    

private:
    QStringList          m_cols;
    QVector<QStringList> m_sampleRows;
    QTextEdit*           m_editor    = nullptr;
    QTextEdit*           m_formatReq = nullptr;
    QLineEdit*           m_titleEdit = nullptr;
    QLabel*              m_status    = nullptr;
    QListWidget*         m_statList  = nullptr;
    QComboBox*           m_groupBox  = nullptr;
    QPushButton*         m_btnAI     = nullptr;
    AIAssistant*         m_ai        = nullptr;
    QPushButton* m_btnFix = nullptr;
};