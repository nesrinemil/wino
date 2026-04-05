#include "chatbubble.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGraphicsDropShadowEffect>
#include <QMap>
#include <QTimer>
#include <QApplication>
#include <QScreen>

ChatBubble::ChatBubble(const QString &message, const QString &time,
                       BubbleType type, QWidget *parent)
    : QWidget(parent), m_type(type), reactionTriggerBtn(nullptr),
      reactionBar(nullptr), reactionsDisplay(nullptr), reactionsLayout(nullptr)
{
    QVBoxLayout *wrapperLayout = new QVBoxLayout(this);
    wrapperLayout->setContentsMargins(0, 0, 0, 0);
    wrapperLayout->setSpacing(2);

    // --- Bubble row ---
    QWidget *bubbleRow = new QWidget(this);
    bubbleRow->setObjectName("bubbleRow");
    bubbleRow->setStyleSheet("QWidget#bubbleRow { background: transparent; }");
    QHBoxLayout *outerLayout = new QHBoxLayout(bubbleRow);
    outerLayout->setContentsMargins(16, 3, 16, 3);
    outerLayout->setSpacing(4);

    if (type == System) {
        QLabel *sysLabel = new QLabel(message, this);
        sysLabel->setAlignment(Qt::AlignCenter);
        sysLabel->setStyleSheet(
            "QLabel { color: #b2bec3; font-size: 12px; background: rgba(0,0,0,0.03);"
            "  border-radius: 12px; padding: 6px 16px; }"
        );
        outerLayout->addStretch();
        outerLayout->addWidget(sysLabel);
        outerLayout->addStretch();
        wrapperLayout->addWidget(bubbleRow);
        return;
    }

    // Create the bubble widget
    QWidget *bubbleWidget = new QWidget(this);
    bubbleWidget->setObjectName("bubbleContent");
    bubbleWidget->setMaximumWidth(420);
    bubbleWidget->setMinimumWidth(80);

    QVBoxLayout *bubbleLayout = new QVBoxLayout(bubbleWidget);
    bubbleLayout->setContentsMargins(14, 10, 14, 8);
    bubbleLayout->setSpacing(4);

    messageLabel = new QLabel(message, bubbleWidget);
    messageLabel->setWordWrap(true);
    messageLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    QHBoxLayout *bottomRow = new QHBoxLayout();
    bottomRow->setSpacing(6);
    bottomRow->addStretch();
    timeLabel = new QLabel(time, bubbleWidget);

    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(bubbleWidget);
    shadow->setBlurRadius(12);
    shadow->setOffset(0, 2);

    if (type == Sent) {
        bubbleWidget->setStyleSheet(
            "QWidget#bubbleContent { background: #00b894; border-radius: 16px;"
            "  border-bottom-right-radius: 4px; }"
        );
        messageLabel->setStyleSheet(
            "QLabel { color: white; font-size: 13px; background: transparent; border: none; }"
        );
        timeLabel->setStyleSheet(
            "QLabel { font-size: 10px; color: rgba(255,255,255,0.65); background: transparent; border: none; }"
        );
        QLabel *status = new QLabel(QString::fromUtf8("\xe2\x9c\x93\xe2\x9c\x93"), bubbleWidget);
        status->setStyleSheet(
            "QLabel { font-size: 10px; color: rgba(255,255,255,0.8); background: transparent; border: none; }"
        );
        bottomRow->addWidget(status);
        shadow->setColor(QColor(0, 184, 148, 50));
        bubbleWidget->setGraphicsEffect(shadow);
    } else {
        bubbleWidget->setStyleSheet(
            "QWidget#bubbleContent { background: #ffffff; border: 1px solid #f0f0f0;"
            "  border-radius: 16px; border-bottom-left-radius: 4px; }"
        );
        messageLabel->setStyleSheet(
            "QLabel { color: #2d3436; font-size: 13px; background: transparent; border: none; }"
        );
        timeLabel->setStyleSheet(
            "QLabel { font-size: 10px; color: #b2bec3; background: transparent; border: none; }"
        );
        shadow->setColor(QColor(0, 0, 0, 20));
        bubbleWidget->setGraphicsEffect(shadow);
    }

    bubbleLayout->addWidget(messageLabel);
    bottomRow->addWidget(timeLabel);
    bubbleLayout->addLayout(bottomRow);

    // Reaction trigger button - RIGHT NEXT to the bubble (not in a separate row)
    reactionTriggerBtn = new QPushButton(QString::fromUtf8("\xf0\x9f\x98\x8a"), bubbleRow);
    reactionTriggerBtn->setFixedSize(26, 26);
    reactionTriggerBtn->setCursor(Qt::PointingHandCursor);
    reactionTriggerBtn->setStyleSheet(
        "QPushButton { background: #f0f2f5; border: 1px solid #e0e0e0; border-radius: 13px;"
        "  font-size: 13px; padding: 0; }"
        "QPushButton:hover { background: #e0e3e8; }"
    );
    reactionTriggerBtn->setVisible(false);
    connect(reactionTriggerBtn, &QPushButton::clicked, this, &ChatBubble::showReactionPicker);

    // Layout order: reaction btn next to bubble
    if (type == Sent) {
        outerLayout->addStretch();
        outerLayout->addWidget(reactionTriggerBtn, 0, Qt::AlignBottom);
        outerLayout->addWidget(bubbleWidget);
    } else {
        outerLayout->addWidget(bubbleWidget);
        outerLayout->addWidget(reactionTriggerBtn, 0, Qt::AlignBottom);
        outerLayout->addStretch();
    }

    wrapperLayout->addWidget(bubbleRow);

    // Reactions display row
    reactionsDisplay = new QWidget(this);
    reactionsDisplay->setObjectName("reactionsRow");
    reactionsDisplay->setStyleSheet("QWidget#reactionsRow { background: transparent; }");
    reactionsLayout = new QHBoxLayout(reactionsDisplay);
    reactionsLayout->setContentsMargins(type == Sent ? 0 : 30, 0, type == Sent ? 30 : 0, 0);
    reactionsLayout->setSpacing(4);
    if (type == Sent) {
        reactionsLayout->addStretch();
    }
    reactionsDisplay->setVisible(false);
    wrapperLayout->addWidget(reactionsDisplay);
}

ChatBubble* ChatBubble::createSystemMessage(const QString &text, QWidget *parent)
{
    return new ChatBubble(text, "", System, parent);
}

void ChatBubble::enterEvent(QEnterEvent *event)
{
    Q_UNUSED(event);
    if (reactionTriggerBtn && m_type != System)
        reactionTriggerBtn->setVisible(true);
}

void ChatBubble::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);
    if (reactionTriggerBtn && m_type != System)
        reactionTriggerBtn->setVisible(false);
    if (reactionBar) {
        QTimer::singleShot(400, this, [this]() {
            if (reactionBar && !reactionBar->underMouse()) {
                reactionBar->deleteLater();
                reactionBar = nullptr;
            }
        });
    }
}

void ChatBubble::showReactionPicker()
{
    if (reactionBar) {
        reactionBar->deleteLater();
        reactionBar = nullptr;
        return;
    }

    // Create as a top-level popup so it floats above everything
    reactionBar = new QWidget(nullptr, Qt::Popup | Qt::FramelessWindowHint);
    reactionBar->setAttribute(Qt::WA_TranslucentBackground);
    reactionBar->setObjectName("reactionPicker");

    QWidget *innerBar = new QWidget(reactionBar);
    innerBar->setObjectName("reactionPickerInner");
    innerBar->setStyleSheet(
        "QWidget#reactionPickerInner { background: white; border: 1px solid #e0e0e0; border-radius: 20px; }"
    );
    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(innerBar);
    shadow->setBlurRadius(20);
    shadow->setColor(QColor(0, 0, 0, 40));
    shadow->setOffset(0, 4);
    innerBar->setGraphicsEffect(shadow);

    QHBoxLayout *outerLay = new QHBoxLayout(reactionBar);
    outerLay->setContentsMargins(8, 8, 8, 8);
    outerLay->addWidget(innerBar);

    QHBoxLayout *barLayout = new QHBoxLayout(innerBar);
    barLayout->setContentsMargins(8, 6, 8, 6);
    barLayout->setSpacing(2);

    QStringList emojis = {
        QString::fromUtf8("\xe2\x9d\xa4\xef\xb8\x8f"),
        QString::fromUtf8("\xf0\x9f\x91\x8d"),
        QString::fromUtf8("\xf0\x9f\x91\x8e"),
        QString::fromUtf8("\xf0\x9f\x98\x82"),
        QString::fromUtf8("\xf0\x9f\x98\xae"),
        QString::fromUtf8("\xf0\x9f\x98\xa2"),
        QString::fromUtf8("\xf0\x9f\x94\xa5")
    };

    for (const auto &emoji : emojis) {
        QPushButton *btn = new QPushButton(emoji, innerBar);
        btn->setFixedSize(36, 36);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(
            "QPushButton { background: transparent; border: none; border-radius: 18px;"
            "  font-size: 18px; padding: 0; }"
            "QPushButton:hover { background: #f0f2f5; }"
        );
        connect(btn, &QPushButton::clicked, this, [this, emoji]() {
            onReactionClicked(emoji);
        });
        barLayout->addWidget(btn);
    }

    reactionBar->adjustSize();

    // Position above the trigger button using GLOBAL coordinates
    QPoint btnGlobal = reactionTriggerBtn->mapToGlobal(QPoint(0, 0));
    int pickerW = reactionBar->sizeHint().width();
    int pickerH = reactionBar->sizeHint().height();

    int x = btnGlobal.x() - pickerW / 2 + reactionTriggerBtn->width() / 2;
    int y = btnGlobal.y() - pickerH - 2;

    // Keep on screen
    QScreen *screen = QApplication::primaryScreen();
    if (screen) {
        QRect geo = screen->availableGeometry();
        if (x < geo.left()) x = geo.left() + 4;
        if (x + pickerW > geo.right()) x = geo.right() - pickerW - 4;
        if (y < geo.top()) y = btnGlobal.y() + reactionTriggerBtn->height() + 4;
    }

    reactionBar->move(x, y);
    reactionBar->show();
}

void ChatBubble::onReactionClicked(const QString &emoji)
{
    if (m_reactions.contains(emoji)) {
        m_reactions.remove(emoji);
    } else {
        m_reactions[emoji] = 1;
    }

    if (reactionBar) {
        reactionBar->close();
        reactionBar->deleteLater();
        reactionBar = nullptr;
    }
    if (reactionTriggerBtn)
        reactionTriggerBtn->setVisible(false);

    updateReactionsDisplay();
}

void ChatBubble::updateReactionsDisplay()
{
    QLayoutItem *child;
    while (reactionsLayout->count() > 0) {
        child = reactionsLayout->takeAt(0);
        if (child->widget()) child->widget()->deleteLater();
        delete child;
    }

    if (m_reactions.isEmpty()) {
        reactionsDisplay->setVisible(false);
        return;
    }

    if (m_type == Sent) {
        reactionsLayout->addStretch();
    }

    for (auto it = m_reactions.constBegin(); it != m_reactions.constEnd(); ++it) {
        QPushButton *badge = new QPushButton(it.key(), reactionsDisplay);
        badge->setCursor(Qt::PointingHandCursor);
        badge->setStyleSheet(
            "QPushButton { background: #e8f8f5; border: 1px solid #00b894; border-radius: 12px;"
            "  padding: 3px 10px; font-size: 14px; }"
            "QPushButton:hover { background: #d5f5ee; border-color: #00a884; }"
        );
        connect(badge, &QPushButton::clicked, this, [this, emoji = it.key()]() {
            onReactionClicked(emoji);
        });
        reactionsLayout->addWidget(badge);
    }

    if (m_type != Sent) {
        reactionsLayout->addStretch();
    }

    reactionsDisplay->setVisible(true);
}
