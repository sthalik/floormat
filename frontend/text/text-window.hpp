#pragma once

#include "cell.hpp"
#include "world-data.hpp"
#include "constants.hpp"

#include <QMainWindow>
#include <QFont>
#include <QFontMetrics>
#include <QSize>
#include <QPainter>

class text_window : public QMainWindow
{
    Q_OBJECT

    QFont monospace_font;
    QFontMetrics font_metrics;
    const QSize font_size;
    const QSize window_size;
    world_data world;

    void paintEvent(QPaintEvent* e) override;
    void draw_cell(QPainter*painter, const cell& c, int x, int y);
public:
    text_window();
};
