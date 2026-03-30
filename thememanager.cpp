#include "thememanager.h"

ThemeManager* ThemeManager::s_instance = nullptr;

ThemeManager* ThemeManager::instance()
{
    if (!s_instance) {
        s_instance = new ThemeManager();
    }
    return s_instance;
}

ThemeManager::ThemeManager(QObject *parent)
    : QObject(parent)
    , m_settings("WINO", "DrivingSchool")
{
    loadTheme();
}

ThemeManager::~ThemeManager()
{
}

void ThemeManager::loadTheme()
{
    int themeValue = m_settings.value("theme", Light).toInt();
    m_currentTheme = static_cast<Theme>(themeValue);
}

void ThemeManager::saveTheme()
{
    m_settings.setValue("theme", static_cast<int>(m_currentTheme));
}

void ThemeManager::setTheme(Theme theme)
{
    if (m_currentTheme != theme) {
        m_currentTheme = theme;
        saveTheme();
        emit themeChanged(theme);
    }
}

void ThemeManager::toggleTheme()
{
    setTheme(m_currentTheme == Light ? Dark : Light);
}

// ══════════════ COLOR GETTERS ══════════════

QString ThemeManager::backgroundColor() const
{
    return m_currentTheme == Light ? "#E5E7EB" : "#0F172A";
}

QString ThemeManager::surfaceColor() const
{
    return m_currentTheme == Light ? "#F9FAFB" : "#1E293B";
}

QString ThemeManager::cardColor() const
{
    return m_currentTheme == Light ? "#FFFFFF" : "#1E293B";
}

QString ThemeManager::primaryTextColor() const
{
    return m_currentTheme == Light ? "#111827" : "#F1F5F9";
}

QString ThemeManager::secondaryTextColor() const
{
    return m_currentTheme == Light ? "#6B7280" : "#94A3B8";
}

QString ThemeManager::accentColor() const
{
    return "#14B8A6"; // Teal stays the same in both themes
}

QString ThemeManager::accentHoverColor() const
{
    return "#0D9488"; // Darker teal for hover
}

QString ThemeManager::borderColor() const
{
    return m_currentTheme == Light ? "#E5E7EB" : "#334155";
}


QString ThemeManager::headerColor() const
{
    return m_currentTheme == Light ? "#FFFFFF" : "#1E293B";
}

QString ThemeManager::headerTextColor() const
{
    return m_currentTheme == Light ? "#111827" : "#FFFFFF"; // primary text color in light
}

QString ThemeManager::headerSecondaryTextColor() const
{
    return m_currentTheme == Light ? "#6B7280" : "#9CA3AF"; // secondary text color in light
}

QString ThemeManager::headerIconBgColor() const
{
    return m_currentTheme == Light ? "rgba(0, 0, 0, 0.05)" : "rgba(255, 255, 255, 0.1)";
}

QString ThemeManager::mutedTextColor() const
{
    return m_currentTheme == Light ? "#9CA3AF" : "#64748B";
}

QString ThemeManager::successColor() const
{
    return m_currentTheme == Light ? "#10B981" : "#34D399";
}

QString ThemeManager::warningColor() const
{
    return m_currentTheme == Light ? "#F97316" : "#FB923C";
}

QString ThemeManager::errorColor() const
{
    return m_currentTheme == Light ? "#EF4444" : "#F87171";
}

QString ThemeManager::tealLight() const
{
    return m_currentTheme == Light ? "#F0FDFA" : "#134E4A";
}

QString ThemeManager::tealLighter() const
{
    return m_currentTheme == Light ? "#CCFBF1" : "#115E59";
}

QString ThemeManager::grayLight() const
{
    return m_currentTheme == Light ? "#F3F4F6" : "#334155";
}

QString ThemeManager::grayLighter() const
{
    return m_currentTheme == Light ? "#F9FAFB" : "#1E293B";
}
