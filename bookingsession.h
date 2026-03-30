#ifndef BOOKINGSESSION_H
#define BOOKINGSESSION_H

#include <QDialog>
#include <QComboBox>
#include <QDateEdit>
#include <QTimeEdit>
#include <QPushButton>
#include <QLabel>
#include <QRadioButton>
#include <QButtonGroup>
#include <QFrame>
#include <QSet>



class BookingSession : public QDialog
{
    Q_OBJECT

public:
    explicit BookingSession(QWidget *parent = nullptr);
    ~BookingSession();

signals:
    void sessionBooked();

private slots:
    void onProceedToPayment();
    void onCancel();
    void onThemeChanged();
    void updateAvailableTimes();

private:

    void setupUI();
    QWidget* createSessionTypeButton(const QString& type, const QString& status, bool enabled);
    void updateColors();
    
    QWidget* dialogWidget;
};

#endif // BOOKINGSESSION_H
