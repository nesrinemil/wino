#ifndef CHATBUBBLE_H
#define CHATBUBBLE_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>

class ChatBubble : public QWidget
{
    Q_OBJECT
public:
    enum BubbleType { Sent, Received, System };

    explicit ChatBubble(const QString &message, const QString &time,
                        BubbleType type, QWidget *parent = nullptr);

    static ChatBubble* createSystemMessage(const QString &text, QWidget *parent = nullptr);

protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private slots:
    void onReactionClicked(const QString &emoji);
    void showReactionPicker();

private:
    QLabel *messageLabel;
    QLabel *timeLabel;
    BubbleType m_type;
    QPushButton *reactionTriggerBtn;
    QWidget *reactionBar;
    QWidget *reactionsDisplay;
    QHBoxLayout *reactionsLayout;
    QMap<QString, int> m_reactions;

    void updateReactionsDisplay();
};

#endif // CHATBUBBLE_H
