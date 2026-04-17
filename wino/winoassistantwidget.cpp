#include "winoassistantwidget.h"
#include "chatbubble.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollBar>
#include <QScrollArea>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QProcess>
#include <QTemporaryFile>
#include <QSettings>
#include <QTimer>
#include <QTime>
#include <QDate>
#include <QDialog>
#include <QTextEdit>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

// ═══════════════════════════════════════════════════════════════════
//  WinoAssistantWidget — AI chatbot powered by Gemini via curl.exe
// ═══════════════════════════════════════════════════════════════════

WinoAssistantWidget::WinoAssistantWidget(const QString &userName,
                                          const QString &userRole,
                                          QWidget *parent)
    : QWidget(parent),
      m_userName(userName), m_userRole(userRole),
      m_isMoniteur(userRole == "Moniteur"),
      m_chatScroll(nullptr), m_chatContent(nullptr), m_chatLayout(nullptr),
      m_input(nullptr), m_sendBtn(nullptr), m_typingLabel(nullptr),
      m_settingsBtn(nullptr),
      m_proc(nullptr), m_tmpPayload(nullptr)
{
    m_apiKey = loadBotApiKey();
    setupUI();
    applyTheme();
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](){ applyTheme(); });
}

WinoAssistantWidget::~WinoAssistantWidget()
{
    if (m_proc && m_proc->state() != QProcess::NotRunning)
        m_proc->kill();
    delete m_tmpPayload;
}

// ── UI ──────────────────────────────────────────────────────────────

void WinoAssistantWidget::setupUI()
{
    QVBoxLayout *mainL = new QVBoxLayout(this);
    mainL->setContentsMargins(0,0,0,0);
    mainL->setSpacing(0);

    // ── Header ──
    QFrame *header = new QFrame(this);
    header->setFixedHeight(70);
    header->setObjectName("waHeader");
    header->setStyleSheet(
        "QFrame#waHeader{"
        "  background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "  stop:0 #00b894, stop:1 #00cec9);"
        "  border: none;"
        "}");

    QHBoxLayout *hL = new QHBoxLayout(header);
    hL->setContentsMargins(20,0,20,0);
    hL->setSpacing(14);

    // Avatar
    QLabel *avatar = new QLabel(header);
    avatar->setFixedSize(44,44);
    avatar->setAlignment(Qt::AlignCenter);
    avatar->setStyleSheet(
        "QLabel{background:#fff;border-radius:22px;font-size:22px;border:none;}");
    avatar->setText(QString::fromUtf8("\xf0\x9f\xa4\x96"));
    hL->addWidget(avatar);

    // Name + status
    QVBoxLayout *nameL = new QVBoxLayout();
    nameL->setSpacing(2);
    QLabel *nameLabel = new QLabel("Assistant Wino", header);
    nameLabel->setStyleSheet(
        "QLabel{font-size:16px;font-weight:bold;color:white;"
        "background:transparent;border:none;}");
    nameL->addWidget(nameLabel);

    QHBoxLayout *statusRow = new QHBoxLayout();
    statusRow->setSpacing(5);
    QLabel *dot = new QLabel(QString::fromUtf8("\xf0\x9f\xa4\x96"), header);
    dot->setStyleSheet("QLabel{font-size:11px;background:transparent;border:none;}");
    statusRow->addWidget(dot);
    QLabel *statusLbl = new QLabel("AI Gemini — Online", header);
    statusLbl->setStyleSheet(
        "QLabel{font-size:11px;color:rgba(255,255,255,0.85);"
        "background:transparent;border:none;}");
    statusRow->addWidget(statusLbl);
    statusRow->addStretch();
    nameL->addLayout(statusRow);

    hL->addLayout(nameL, 1);

    // Settings gear
    m_settingsBtn = new QPushButton(header);
    m_settingsBtn->setFixedSize(36,36);
    m_settingsBtn->setCursor(Qt::PointingHandCursor);
    m_settingsBtn->setToolTip("Configure API key");
    m_settingsBtn->setStyleSheet(
        "QPushButton{background:rgba(255,255,255,0.15);border:none;border-radius:18px;"
        "font-size:18px;color:white;}"
        "QPushButton:hover{background:rgba(255,255,255,0.30);}");
    m_settingsBtn->setText(QString::fromUtf8("\xe2\x9a\x99\xef\xb8\x8f"));
    connect(m_settingsBtn, &QPushButton::clicked,
            this, &WinoAssistantWidget::showApiKeyDialog);
    hL->addWidget(m_settingsBtn);

    mainL->addWidget(header);

    // ── Chat area ──
    m_chatScroll = new QScrollArea(this);
    m_chatScroll->setWidgetResizable(true);
    m_chatScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_chatScroll->setStyleSheet(
        "QScrollArea{background:#f0f2f5;border:none;}"
        "QScrollBar:vertical{width:5px;background:#f0f2f5;}"
        "QScrollBar::handle:vertical{background:#ccc;border-radius:3px;}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}");

    m_chatContent = new QWidget();
    m_chatContent->setStyleSheet("background:#f0f2f5;");
    m_chatLayout = new QVBoxLayout(m_chatContent);
    m_chatLayout->setContentsMargins(8,12,8,12);
    m_chatLayout->setSpacing(4);
    m_chatLayout->addStretch();

    m_chatScroll->setWidget(m_chatContent);
    mainL->addWidget(m_chatScroll, 1);

    // Welcome + system date separator
    addSystemMessage(QDate::currentDate().toString("—  Today  —"));
    addWelcomeBubble();

    // ── Typing indicator ──
    m_typingLabel = new QLabel(this);
    m_typingLabel->setText(QString::fromUtf8(
        "  \xf0\x9f\xa4\x96  Assistant Wino is typing..."));
    m_typingLabel->setStyleSheet(
        "QLabel{color:#636e72;font-size:11px;background:#f0f2f5;"
        "padding:4px 16px;border:none;}");
    m_typingLabel->setVisible(false);
    mainL->addWidget(m_typingLabel);

    // ── Input bar ──
    m_inputBar = new QFrame(this);
    m_inputBar->setObjectName("waInputBar");
    m_inputBar->setFixedHeight(64);
    m_inputBar->setStyleSheet(
        "QFrame#waInputBar{background:white;"
        "border-top:1px solid #f0f0f0;border-radius:0;}");
    QFrame *inputBar = m_inputBar;

    QHBoxLayout *inputL = new QHBoxLayout(inputBar);
    inputL->setContentsMargins(12,10,12,10);
    inputL->setSpacing(8);

    // Emoji button (decorative)
    QPushButton *emojiBtn = new QPushButton(inputBar);
    emojiBtn->setFixedSize(36,36);
    emojiBtn->setCursor(Qt::PointingHandCursor);
    emojiBtn->setText(QString::fromUtf8("\xf0\x9f\x98\x8a"));
    emojiBtn->setStyleSheet(
        "QPushButton{background:transparent;border:none;font-size:18px;}"
        "QPushButton:hover{background:#f0f2f5;border-radius:18px;}");
    inputL->addWidget(emojiBtn);

    // Mic button — Windows speech recognition
    m_micBtn = new QPushButton(inputBar);
    m_micBtn->setFixedSize(36,36);
    m_micBtn->setCursor(Qt::PointingHandCursor);
    m_micBtn->setText(QString::fromUtf8("\xf0\x9f\x8e\xa4"));
    m_micBtn->setToolTip("Click to speak");
    m_micBtn->setStyleSheet(
        "QPushButton{background:transparent;border:none;font-size:18px;}"
        "QPushButton:hover{background:#f0f2f5;border-radius:18px;}");
    connect(m_micBtn, &QPushButton::clicked,
            this, &WinoAssistantWidget::onMicClicked);
    inputL->addWidget(m_micBtn);

    m_input = new QLineEdit(inputBar);
    m_input->setPlaceholderText("Ask the assistant your question...");
    m_input->setStyleSheet(
        "QLineEdit{background:#f8f9fa;border:1.5px solid #e2e8f0;"
        "border-radius:20px;padding:8px 16px;font-size:13px;color:#2d3436;}"
        "QLineEdit:focus{border-color:#00b894;background:white;}");
    connect(m_input, &QLineEdit::returnPressed,
            this, &WinoAssistantWidget::onSendMessage);
    inputL->addWidget(m_input, 1);

    // Wink button (decorative)
    QPushButton *winkBtn = new QPushButton(inputBar);
    winkBtn->setFixedSize(36,36);
    winkBtn->setCursor(Qt::PointingHandCursor);
    winkBtn->setText(QString::fromUtf8("\xf0\x9f\x93\x8e"));
    winkBtn->setStyleSheet(
        "QPushButton{background:transparent;border:none;font-size:18px;}"
        "QPushButton:hover{background:#f0f2f5;border-radius:18px;}");
    inputL->addWidget(winkBtn);

    m_sendBtn = new QPushButton(inputBar);
    m_sendBtn->setFixedSize(40,40);
    m_sendBtn->setCursor(Qt::PointingHandCursor);
    m_sendBtn->setText(QString::fromUtf8("\xe2\x9e\xa4"));
    m_sendBtn->setStyleSheet(
        "QPushButton{background:#00b894;color:white;border:none;"
        "border-radius:20px;font-size:18px;}"
        "QPushButton:hover{background:#00a884;}"
        "QPushButton:pressed{background:#009874;}");
    connect(m_sendBtn, &QPushButton::clicked,
            this, &WinoAssistantWidget::onSendMessage);
    inputL->addWidget(m_sendBtn);

    mainL->addWidget(inputBar);
}

void WinoAssistantWidget::addWelcomeBubble()
{
    QString welcome = QString::fromUtf8(
        "Bonjour ! \xf0\x9f\x91\x8b Je suis l\xe2\x80\x99""Assistant Wino, "
        "votre aide intelligent assistant for the driving school.\n\n"
        "Posez-moi vos questions sur :\n"
        "\xf0\x9f\x93\x8b Le code de la route\n"
        "\xf0\x9f\x9a\x97 La conduite pratique\n"
        "\xf0\x9f\x8e\x93 L\xe2\x80\x99""examen du permis\n"
        "\xf0\x9f\x9a\xa6 Les panneaux et signalisation\n\n"
        "Comment puis-je vous aider aujourd\xe2\x80\x99""hui ?");

    QString time = QTime::currentTime().toString("HH:mm");
    auto *bubble = new ChatBubble(welcome, time, ChatBubble::Received, m_chatContent);
    m_chatLayout->insertWidget(m_chatLayout->count() - 1, bubble);
}

void WinoAssistantWidget::addBubble(const QString &text, bool isSent)
{
    QString time = QTime::currentTime().toString("HH:mm");
    auto type = isSent ? ChatBubble::Sent : ChatBubble::Received;
    auto *bubble = new ChatBubble(text, time, type, m_chatContent);
    m_chatLayout->insertWidget(m_chatLayout->count() - 1, bubble);
    QTimer::singleShot(30, this, [this](){ scrollToBottom(); });
}

void WinoAssistantWidget::addSystemMessage(const QString &text)
{
    auto *msg = ChatBubble::createSystemMessage(text, m_chatContent);
    m_chatLayout->insertWidget(m_chatLayout->count() - 1, msg);
}

void WinoAssistantWidget::showTyping(bool on)
{
    m_typingLabel->setVisible(on);
    m_sendBtn->setEnabled(!on);
    m_input->setEnabled(!on);
    if (on) scrollToBottom();
}

void WinoAssistantWidget::scrollToBottom()
{
    m_chatScroll->verticalScrollBar()->setValue(
        m_chatScroll->verticalScrollBar()->maximum());
}

// ── Send / Receive ───────────────────────────────────────────────────

void WinoAssistantWidget::onSendMessage()
{
    QString msg = m_input->text().trimmed();
    if (msg.isEmpty()) return;

    m_input->clear();
    addBubble(msg, true);
    sendToGemini(msg);
}

void WinoAssistantWidget::sendToGemini(const QString &userMsg)
{
    if (m_apiKey.isEmpty()) {
        showApiKeyDialog();
        if (m_apiKey.isEmpty()) {
            addBubble(QString::fromUtf8(
                "\xe2\x9a\xa0\xef\xb8\x8f Please configure your Gemini API key "
                "via the \xe2\x9a\x99\xef\xb8\x8f button at the top."), false);
            return;
        }
    }

    // Build multi-turn history entry
    QJsonObject userTurn;
    userTurn["role"] = "user";
    QJsonArray userParts;
    QJsonObject ut; ut["text"] = userMsg;
    userParts.append(ut);
    userTurn["parts"] = userParts;
    m_history.append(userTurn);

    // System instruction
    QString sysPrompt = m_isMoniteur ?
        QString::fromUtf8(
            "You are the Wino Assistant, an intelligent chatbot for the Wino Smart "
            "Driving School app in Tunisia. You help the driving school with: "
            "managing sessions and planning, Tunisian highway code and traffic rules, "
            "pedagogical tips for teaching driving, preparing students for exams, "
            "traffic signs, and administrative procedures. "
            "Respond in English clearly and professionally. Use emojis to be friendly.") :
        QString::fromUtf8(
            "You are the Wino Assistant, an intelligent chatbot for the Wino Smart "
            "Driving School app in Tunisia. You help the student with: "
            "Tunisian highway code and traffic rules, practical driving tips, "
            "preparing for theory and practical exams, traffic signs, priorities, "
            "roundabouts, highway driving, and administrative procedures for the license. "
            "Respond in English clearly and encouragingly. Use emojis to be friendly.");

    QJsonObject systemInstruction;
    QJsonArray sysParts;
    QJsonObject sysText; sysText["text"] = sysPrompt;
    sysParts.append(sysText);
    systemInstruction["parts"] = sysParts;

    QJsonObject body;
    body["contents"]          = m_history;
    body["systemInstruction"] = systemInstruction;
    QJsonObject genCfg;
    genCfg["temperature"]    = 0.7;
    genCfg["maxOutputTokens"] = 1024;
    body["generationConfig"] = genCfg;

    QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);

    // Write payload to temp file
    if (m_tmpPayload) { m_tmpPayload->close(); delete m_tmpPayload; }
    m_tmpPayload = new QTemporaryFile(this);
    m_tmpPayload->setAutoRemove(true);
    if (!m_tmpPayload->open()) {
        addBubble(QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f Temp file error."), false);
        return;
    }
    m_tmpPayload->write(payload);
    m_tmpPayload->flush();
    QString payloadPath = m_tmpPayload->fileName();

    // Kill any in-flight request
    if (m_proc && m_proc->state() != QProcess::NotRunning) m_proc->kill();
    delete m_proc;
    m_proc = new QProcess(this);
    connect(m_proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &WinoAssistantWidget::onAIFinished);

    QString endpoint = QString(
        "https://generativelanguage.googleapis.com/v1beta/models/"
        "gemini-2.5-flash:generateContent");

    QStringList args;
    args << "-s" << "-X" << "POST" << endpoint
         << "-H" << "Content-Type: application/json"
         << "-H" << QString("x-goog-api-key: %1").arg(m_apiKey)
         << "-d" << QString("@%1").arg(payloadPath);

    showTyping(true);
    m_proc->start("C:/Windows/System32/curl.exe", args);
}

void WinoAssistantWidget::onAIFinished(int /*exitCode*/, int /*exitStatus*/)
{
    showTyping(false);

    QByteArray raw = m_proc->readAllStandardOutput();
    QJsonDocument doc = QJsonDocument::fromJson(raw);
    QString responseText;

    if (!doc.isNull()) {
        QJsonObject obj = doc.object();
        if (obj.contains("candidates")) {
            QJsonArray cands = obj["candidates"].toArray();
            if (!cands.isEmpty()) {
                QJsonArray parts = cands[0].toObject()["content"]
                                          .toObject()["parts"].toArray();
                for (const auto &p : parts)
                    responseText += p.toObject()["text"].toString();

                // Add to multi-turn history
                QJsonObject modelTurn;
                modelTurn["role"] = "model";
                QJsonArray mParts;
                QJsonObject mt; mt["text"] = responseText;
                mParts.append(mt);
                modelTurn["parts"] = mParts;
                m_history.append(modelTurn);

                // Keep last 20 turns
                while (m_history.size() > 20) m_history.removeFirst();
            }
        } else if (obj.contains("error")) {
            QString errMsg = obj["error"].toObject()["message"].toString();
            responseText = QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f Error: ") + errMsg;
            if (errMsg.contains("API_KEY") || errMsg.contains("key"))
                m_apiKey.clear(); // force re-entry on next send
        }
    }

    if (responseText.isEmpty())
        responseText = QString::fromUtf8(
            "\xe2\x9a\xa0\xef\xb8\x8f No response. Check your API key or internet connection.");

    addBubble(responseText, false);
}

// ── API Key dialog ───────────────────────────────────────────────────

void WinoAssistantWidget::showApiKeyDialog()
{
    QDialog *dlg = new QDialog(this);
    dlg->setWindowTitle("Configure Gemini API Key");
    dlg->setFixedSize(480, 300);
    dlg->setStyleSheet("QDialog{background:white;}");
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    QVBoxLayout *lay = new QVBoxLayout(dlg);
    lay->setContentsMargins(28,22,28,22);
    lay->setSpacing(12);

    QLabel *icon = new QLabel(QString::fromUtf8("\xf0\x9f\xa4\x96"), dlg);
    icon->setStyleSheet("QLabel{font-size:42px;background:transparent;border:none;}");
    icon->setAlignment(Qt::AlignCenter);
    lay->addWidget(icon);

    QLabel *title = new QLabel("Configure the Wino Assistant", dlg);
    title->setStyleSheet(
        "QLabel{font-size:17px;font-weight:bold;color:#2d3436;"
        "background:transparent;border:none;}");
    title->setAlignment(Qt::AlignCenter);
    lay->addWidget(title);

    QLabel *desc = new QLabel(
        "Enter your Google Gemini API key.\nGet it free at aistudio.google.com/apikey", dlg);
    desc->setStyleSheet("QLabel{font-size:11px;color:#636e72;background:transparent;border:none;}");
    desc->setAlignment(Qt::AlignCenter);
    lay->addWidget(desc);

    QLineEdit *keyInput = new QLineEdit(dlg);
    keyInput->setPlaceholderText("Paste your API key here...");
    keyInput->setText(m_apiKey);
    keyInput->setEchoMode(QLineEdit::Password);
    keyInput->setStyleSheet(
        "QLineEdit{border:2px solid #e2e8f0;border-radius:12px;"
        "padding:10px 14px;font-size:13px;color:#2d3436;background:#f8f9fa;}"
        "QLineEdit:focus{border-color:#00b894;background:white;}");
    lay->addWidget(keyInput);

    lay->addStretch();

    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->addStretch();

    QPushButton *cancelBtn = new QPushButton("Cancel", dlg);
    cancelBtn->setStyleSheet(
        "QPushButton{border:1px solid #e2e8f0;border-radius:10px;"
        "padding:9px 22px;font-size:12px;color:#636e72;background:white;}"
        "QPushButton:hover{background:#f8f9fa;}");
    connect(cancelBtn, &QPushButton::clicked, dlg, &QDialog::reject);
    btnRow->addWidget(cancelBtn);

    QPushButton *saveBtn = new QPushButton(QString::fromUtf8("\xe2\x9c\x85 Save"), dlg);
    saveBtn->setCursor(Qt::PointingHandCursor);
    saveBtn->setStyleSheet(
        "QPushButton{background:#00b894;color:white;border:none;"
        "border-radius:10px;padding:9px 22px;font-size:12px;font-weight:bold;}"
        "QPushButton:hover{background:#00a884;}");
    connect(saveBtn, &QPushButton::clicked, dlg, [this, dlg, keyInput](){
        QString k = keyInput->text().trimmed();
        if (!k.isEmpty()) {
            m_apiKey = k;
            saveBotApiKey(k);
            dlg->accept();
        }
    });
    btnRow->addWidget(saveBtn);
    lay->addLayout(btnRow);

    dlg->exec();
}

void WinoAssistantWidget::saveBotApiKey(const QString &key)
{
    QSettings s("Wino", "WinoAssistant");
    s.setValue("gemini_api_key", key);
}

QString WinoAssistantWidget::loadBotApiKey() const
{
    QSettings s("Wino", "WinoAssistant");
    return s.value("gemini_api_key", "").toString();
}

// ── Theme ────────────────────────────────────────────────────────────

void WinoAssistantWidget::applyTheme()
{
    auto *tm = ThemeManager::instance();
    bool dark = tm->currentTheme() == ThemeManager::Dark;

    QString chatBg      = dark ? "#0F172A" : "#f0f2f5";
    QString chatHandle  = dark ? "#475569" : "#cccccc";
    QString inputBg     = dark ? "#1E293B" : "#ffffff";
    QString inputBorder = dark ? "#334155" : "#f0f0f0";
    QString inputField  = dark ? "#253548" : "#f8f9fa";
    QString inputFocus  = dark ? "#1E293B" : "#ffffff";
    QString fieldBorder = dark ? "#334155" : "#e2e8f0";
    QString textColor   = dark ? "#F1F5F9" : "#2d3436";
    QString mutedColor  = dark ? "#94A3B8" : "#636e72";
    QString hoverColor  = dark ? "#1E293B" : "#f0f2f5";

    m_chatScroll->setStyleSheet(QString(
        "QScrollArea{background:%1;border:none;}"
        "QScrollBar:vertical{width:5px;background:%1;}"
        "QScrollBar::handle:vertical{background:%2;border-radius:3px;}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
        .arg(chatBg, chatHandle));

    m_chatContent->setStyleSheet(QString("background:%1;").arg(chatBg));

    m_typingLabel->setStyleSheet(QString(
        "QLabel{color:%1;font-size:11px;background:%2;padding:4px 16px;border:none;}")
        .arg(mutedColor, chatBg));

    if (m_inputBar) {
        m_inputBar->setStyleSheet(QString(
            "QFrame#waInputBar{background:%1;border-top:1px solid %2;border-radius:0;}")
            .arg(inputBg, inputBorder));
    }

    m_input->setStyleSheet(QString(
        "QLineEdit{background:%1;border:1.5px solid %2;"
        "border-radius:20px;padding:8px 16px;font-size:13px;color:%3;}"
        "QLineEdit:focus{border-color:#00b894;background:%4;}")
        .arg(inputField, fieldBorder, textColor, inputFocus));

    // Update mic/emoji button hover color
    QString iconBtnSS = QString(
        "QPushButton{background:transparent;border:none;font-size:18px;}"
        "QPushButton:hover{background:%1;border-radius:18px;}").arg(hoverColor);
    if (m_micBtn) m_micBtn->setStyleSheet(iconBtnSS);
}

// ── Microphone — Windows Speech Recognition via PowerShell ──────────

void WinoAssistantWidget::onMicClicked()
{
    if (!m_micBtn) return;
    m_micBtn->setEnabled(false);
    m_micBtn->setText(QString::fromUtf8("\xf0\x9f\x94\xb4")); // red circle = recording

    // Use Windows System.Speech via PowerShell (built-in, no install needed)
    QString script =
        "Add-Type -AssemblyName System.Speech; "
        "$r = New-Object System.Speech.Recognition.SpeechRecognitionEngine; "
        "$r.SetInputToDefaultAudioDevice(); "
        "$r.LoadGrammar((New-Object System.Speech.Recognition.DictationGrammar)); "
        "$result = $r.Recognize([TimeSpan]::FromSeconds(6)); "
        "if ($result) { Write-Output $result.Text }";

    QProcess *proc = new QProcess(this);
    connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc](int, QProcess::ExitStatus) {
        QString text = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        if (!text.isEmpty()) {
            m_input->setText(text);
            m_input->setFocus();
        }
        m_micBtn->setEnabled(true);
        m_micBtn->setText(QString::fromUtf8("\xf0\x9f\x8e\xa4"));
        proc->deleteLater();
    });

    proc->start("powershell", QStringList() << "-NoProfile" << "-Command" << script);
}
