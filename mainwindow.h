#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QVector>
#include <QNetworkAccessManager>
#include <functional>
#include "student.h"

class QPaintEvent;
class QPainter;
class QWidget;
class QLabel;
class QProgressDialog;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

// Floating car data
struct AnimCar {
    qreal x, y;           // position
    qreal speed;          // pixels per frame
    qreal size;           // scale factor
    qreal opacity;        // current opacity 0..1
    qreal targetOpacity;  // fade-in target
    bool  facingRight;    // direction
    int   colorIndex;     // color variant
    int   carType;        // 0=sedan  1=SUV  2=sports
    qreal wheelAngle;     // degrees — spins each frame
    qreal bobPhase;       // vertical oscillation phase (radians)
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    Ui::MainWindow *ui;
    QTimer *animTimer;
    QVector<AnimCar> cars;
    QNetworkAccessManager *networkManager;
    QVector<Student> m_students;

    void initCars();
    void drawCar(QPainter &p, const AnimCar &car);

    // OCR methods
    void startOcrScan(const QString &frontPath, const QString &backPath);
    void sendOcrRequest(const QString &imagePath, const QString &prompt,
                        std::function<void(const QString &text)> callback,
                        QProgressDialog *progress);
    void processOcrResults(const QString &frontText, const QString &backText);

    // Email verification
    QString m_verificationCode;
    bool    m_fingerprintDone = false;
    void sendVerificationEmail(const QString &toEmail, const QString &code,
                               std::function<void(bool, QString)> callback);

    // UI animation helpers
    void showToast(const QString &message, bool isError = true);
    void shakeWidget(QWidget *w);
    void showWidget(QWidget *w);
    void showSuccess(const QString &title, const QString &message);

    // Camera capture
    void openCameraCapture(const QString &instruction,
                           const QString &frameResource,
                           std::function<void(const QString &savedPath)> callback);

    // Local in-memory storage helpers
    bool insertStudentFromForm(QString *errorMessage);
    bool verifyStudentCredentials(const QString &email, const QString &password, QString *fullName);
    QString hashPassword(const QString &password) const;
    void clearRegistrationForm();

private slots:
    void goToStep(int step);
    void updateAnimation();
};

#endif // MAINWINDOW_H
