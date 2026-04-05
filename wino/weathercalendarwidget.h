#ifndef WEATHERCALENDARWIDGET_H
#define WEATHERCALENDARWIDGET_H

#include <QCalendarWidget>
#include <QPainter>
#include <QDate>
#include "weatherservice.h"

class WeatherCalendarWidget : public QCalendarWidget
{
    Q_OBJECT

public:
    explicit WeatherCalendarWidget(QWidget *parent = nullptr)
        : QCalendarWidget(parent)
    {
    }

protected:
    void paintCell(QPainter *painter, const QRect &rect, QDate date) const override
    {
        // Draw default cell first
        QCalendarWidget::paintCell(painter, rect, date);

        // Draw weather icon if available
        WeatherService *weather = WeatherService::instance();
        if (weather->hasData()) {
            DayWeather dw = weather->getWeather(date);
            if (dw.weatherCode >= 0) {
                painter->save();

                // Draw weather emoji in the top-right corner of the cell
                QFont emojiFont = painter->font();
                emojiFont.setPixelSize(12);
                painter->setFont(emojiFont);

                // Position in top-right corner
                QRect iconRect(rect.right() - 18, rect.top() + 2, 16, 16);
                painter->drawText(iconRect, Qt::AlignCenter, dw.icon);

                // Draw temperature in bottom of cell
                QFont tempFont = painter->font();
                tempFont.setPixelSize(9);
                tempFont.setBold(true);
                painter->setFont(tempFont);
                painter->setPen(QColor("#0D9488"));

                QRect tempRect(rect.left() + 2, rect.bottom() - 14, rect.width() - 4, 12);
                QString tempText = QString("%1°").arg(dw.tempMax, 0, 'f', 0);
                painter->drawText(tempRect, Qt::AlignCenter, tempText);

                painter->restore();
            }
        }
    }
};

#endif // WEATHERCALENDARWIDGET_H
