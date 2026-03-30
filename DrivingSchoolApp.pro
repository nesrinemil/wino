QT += core gui sql network multimedia multimediawidgets svg printsupport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# ── Maryem driving school sources ────────────────────────────────────────────
SOURCES += \
    main.cpp \
    mainwindow.cpp \
    student.cpp \
    roleselectionwindow.cpp \
    admindashboard.cpp \
    studentdashboard.cpp \
    instructordashboard.cpp \
    database.cpp \
    emailservice.cpp \
    smartdrivewindow.cpp \
    studentlearninghub.cpp \
    schoolselectionwindow.cpp

# ── Modules WINO (tâche Nesrine) ─────────────────────────────────────────────
SOURCES += \
    thememanager.cpp \
    weatherservice.cpp \
    smtpmailer.cpp \
    paymentverification.cpp \
    bookingsession.cpp \
    airecommendations.cpp \
    emailsender.cpp

HEADERS += \
    mainwindow.h \
    student.h \
    roleselectionwindow.h \
    admindashboard.h \
    studentdashboard.h \
    instructordashboard.h \
    database.h \
    emailservice.h \
    smartdrivewindow.h \
    studentlearninghub.h \
    schoolselectionwindow.h \
    thememanager.h \
    weatherservice.h \
    weathercalendarwidget.h \
    smtpmailer.h \
    paymentverification.h \
    bookingsession.h \
    airecommendations.h \
    emailsender.h

# ── TACHE learning module sources (shared, not copied) ───────────────────────
SOURCES += \
    ../hous/databasemanager.cpp \
    ../hous/loadingscreen.cpp \
    ../hous/codelearningmodule.cpp \
    ../hous/quizwidget.cpp \
    ../hous/vrwidget.cpp \
    ../hous/sidebar.cpp \
    ../hous/circularprogressbar.cpp \
    ../hous/sectioncard.cpp \
    ../hous/animatedstackedwidget.cpp \
    ../hous/twiliowhatsapp.cpp \
    ../hous/trafficsignwidget.cpp

HEADERS += \
    ../hous/databasemanager.h \
    ../hous/loadingscreen.h \
    ../hous/codelearningmodule.h \
    ../hous/quizwidget.h \
    ../hous/vrwidget.h \
    ../hous/sidebar.h \
    ../hous/circularprogressbar.h \
    ../hous/sectioncard.h \
    ../hous/animatedstackedwidget.h \
    ../hous/twiliowhatsapp.h \
    ../hous/trafficsignwidget.h

# Make TACHE headers findable by their short names
INCLUDEPATH += ../hous

# ── Circuit (SmartDrivingSchool) sources ─────────────────────────────────────
SDS = ../han/SmartDrivingSchool

SOURCES += \
    $$SDS/circuitdb.cpp \
    $$SDS/circuitdashboard.cpp \
    $$SDS/csvreader.cpp \
    $$SDS/dataprocessor.cpp \
    $$SDS/recommendationengine.cpp \
    $$SDS/mlpredictor.cpp \
    $$SDS/sessioncomparisonwidget.cpp \
    $$SDS/progresstrackingwidget.cpp \
    $$SDS/pdfreportgenerator.cpp \
    $$SDS/settingsdialog.cpp \
    $$SDS/analyticsengine.cpp \
    $$SDS/studentmanagerdialog.cpp \
    $$SDS/studenthistorywidget.cpp \
    $$SDS/loginwindow.cpp \
    $$SDS/studentportal.cpp \
    $$SDS/qcustomplot.cpp

HEADERS += \
    $$SDS/circuitdb.h \
    $$SDS/circuitdashboard.h \
    $$SDS/csvreader.h \
    $$SDS/dataprocessor.h \
    $$SDS/recommendationengine.h \
    $$SDS/mlpredictor.h \
    $$SDS/sessioncomparisonwidget.h \
    $$SDS/progresstrackingwidget.h \
    $$SDS/pdfreportgenerator.h \
    $$SDS/settingsdialog.h \
    $$SDS/analyticsengine.h \
    $$SDS/studentmanagerdialog.h \
    $$SDS/studenthistorywidget.h \
    $$SDS/loginwindow.h \
    $$SDS/studentportal.h \
    $$SDS/qcustomplot.h

FORMS += $$SDS/circuitdashboard.ui

INCLUDEPATH += $$SDS

FORMS += \
    mainwindow.ui \
    roleselectionwindow.ui \
    admindashboard.ui \
    studentdashboard.ui \
    instructordashboard.ui \
    schoolselectionwindow.ui

RESOURCES += \
    assets.qrc \
    tache_resources.qrc

# Oracle OCI library
win32 {
    LIBS += -LC:\oraclexe\app\oracle\product\11.2.0\server\bin -loci
    INCLUDEPATH += C:\oraclexe\app\oracle\product\11.2.0\server\oci\include

    # qcustomplot.cpp is too large for COFF default section limit — enable big obj
    QMAKE_CXXFLAGS += -Wa,-mbig-obj
}

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
