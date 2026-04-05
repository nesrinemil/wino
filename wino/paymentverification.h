#ifndef PAYMENTVERIFICATION_H
#define PAYMENTVERIFICATION_H

#include <QWidget>
#include <QFrame>
#include <QPushButton>
#include <QLabel>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDate>
#include <QDateTime>



class PaymentVerification : public QWidget
{
    Q_OBJECT

public:
    explicit PaymentVerification(QWidget *parent = nullptr);
    ~PaymentVerification();

private slots:
    void onVerifyPayment(int paymentId, const QString& studentName);
    void onRejectPayment(int paymentId, const QString& studentName);

private:

    void setupUI();
    QWidget* createPaymentCard(int paymentId, const QString& studentName,
                              const QString& transactionCode, const QString& paymentDate,
                              const QString& amount);
};

#endif // PAYMENTVERIFICATION_H
