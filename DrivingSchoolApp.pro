QT += core gui sql network multimedia multimediawidgets svg

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
    smartdrivewindow.cpp

HEADERS += \
    mainwindow.h \
    student.h \
    roleselectionwindow.h \
    admindashboard.h \
    studentdashboard.h \
    instructordashboard.h \
    database.h \
    emailservice.h \
    smartdrivewindow.h

# ── TACHE learning module sources (shared, not copied) ───────────────────────
SOURCES += \
    ../9/databasemanager.cpp \
    ../9/loadingscreen.cpp \
    ../9/codelearningmodule.cpp \
    ../9/quizwidget.cpp \
    ../9/vrwidget.cpp \
    ../9/sidebar.cpp \
    ../9/circularprogressbar.cpp \
    ../9/sectioncard.cpp \
    ../9/animatedstackedwidget.cpp \
    ../9/twiliowhatsapp.cpp \
    ../9/trafficsignwidget.cpp

HEADERS += \
    ../9/databasemanager.h \
    ../9/loadingscreen.h \
    ../9/codelearningmodule.h \
    ../9/quizwidget.h \
    ../9/vrwidget.h \
    ../9/sidebar.h \
    ../9/circularprogressbar.h \
    ../9/sectioncard.h \
    ../9/animatedstackedwidget.h \
    ../9/twiliowhatsapp.h \
    ../9/trafficsignwidget.h

# Make TACHE headers findable by their short names
INCLUDEPATH += ../9

FORMS += \
    mainwindow.ui \
    roleselectionwindow.ui \
    admindashboard.ui \
    studentdashboard.ui \
    instructordashboard.ui

RESOURCES += \
    assets.qrc \
    tache_resources.qrc

# Oracle OCI library
win32 {
    LIBS += -LC:\oraclexe\app\oracle\product\11.2.0\server\bin -loci
    INCLUDEPATH += C:\oraclexe\app\oracle\product\11.2.0\server\oci\include
}

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
