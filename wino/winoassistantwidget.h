#ifndef WINOASSISTANTWIDGET_H
#define WINOASSISTANTWIDGET_H

#include <QWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QJsonArray>

class QProcess;
class QTemporaryFile;

class WinoAssistantWidget : public QWidget
{
    Q_OBJECT
public:
    explicit WinoAssistantWidget(const QString &userName,
                                  const QString &userRole,
                                  QWidget *parent = nullptr);
    ~WinoAssistantWidget();

private slots:
    void onSendMessage();
    void onAIFinished(int exitCode, int exitStatus);
    void showApiKeyDialog();

private:
    void setupUI();
    void addWelcomeBubble();
    void addBubble(const QString &text, bool isSent);
    void addSystemMessage(const QString &text);
    void sendToGemini(const QString &userMsg);
    void showTyping(bool on);
    void scrollToBottom();
    void saveBotApiKey(const QString &key);
    QString loadBotApiKey() const;

    QString m_userName, m_userRole;
    bool    m_isMoniteur;

    QScrollArea   *m_chatScroll;
    QWidget       *m_chatContent;
    QVBoxLayout   *m_chatLayout;
    QLineEdit     *m_input;
    QPushButton   *m_sendBtn;
    QLabel        *m_typingLabel;
    QPushButton   *m_settingsBtn;

    QString        m_apiKey;
    QJsonArray     m_history;    // multi-turn context
    QProcess      *m_proc;
    QTemporaryFile *m_tmpPayload;
};

#endif // WINOASSISTANTWIDGET_H
