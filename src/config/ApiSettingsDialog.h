#pragma once
#include <QDialog>
#include "AIAssistant.h"

class QLineEdit;
class QComboBox;
class QLabel;

class ApiSettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit ApiSettingsDialog(const ApiConfig& current,
                                QWidget* parent = nullptr);
    ApiConfig result() const;

private slots:
    void onPresetChanged(int idx);
    void onTestApi();

private:
    QComboBox* m_preset;
    QComboBox* m_format;
    QLineEdit* m_url;
    QLineEdit* m_key;
    QLineEdit* m_model;
    QLabel*    m_testStatus;
};