#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QObject>
#include <QString>
#include <QColor>
#include <QSettings>

class ThemeManager : public QObject
{
    Q_OBJECT

public:
    enum Theme {
        Light,
        Dark
    };

    static ThemeManager* instance();
    
    Theme currentTheme() const { return m_currentTheme; }
    void setTheme(Theme theme);
    void toggleTheme();
    
    // Color getters for current theme
    QString backgroundColor() const;
    QString surfaceColor() const;
    QString cardColor() const;
    QString primaryTextColor() const;
    QString secondaryTextColor() const;
    QString accentColor() const;
    QString accentHoverColor() const;
    QString borderColor() const;
    QString headerColor() const;
    QString headerTextColor() const;
    QString headerSecondaryTextColor() const;
    QString headerIconBgColor() const;
    QString mutedTextColor() const;
    QString successColor() const;
    QString warningColor() const;
    QString errorColor() const;
    QString tealLight() const;
    QString tealLighter() const;
    QString grayLight() const;
    QString grayLighter() const;
    
signals:
    void themeChanged(Theme newTheme);

private:
    explicit ThemeManager(QObject *parent = nullptr);
    ~ThemeManager();
    ThemeManager(const ThemeManager&) = delete;
    ThemeManager& operator=(const ThemeManager&) = delete;
    
    void loadTheme();
    void saveTheme();
    
    static ThemeManager* s_instance;
    Theme m_currentTheme;
    QSettings m_settings;
};

#endif // THEMEMANAGER_H
