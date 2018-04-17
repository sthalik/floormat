#include "text-window.hpp"
#include "constants.hpp"

#include <algorithm>

#include <QFontDatabase>
#include <QFontMetrics>
#include <QPaintEvent>
#include <QPainter>
#include <QStatusBar>
#include <QDebug>

using namespace constants;

text_window::text_window() :
    //monospace_font(QFont("Segoe UI", font_size)),
    monospace_font(QFont(QFontDatabase::systemFont(QFontDatabase::FixedFont).family(), constants::font_size)),
    font_metrics(QFontMetrics(monospace_font)),
    font_size(font_metrics.width("W"), font_metrics.lineSpacing()),
    nrows(constants::nrows),
    ncols(constants::ncols)
{
    monospace_font.setStyleStrategy(QFont::PreferAntialias);

    // disable resize voodoo #1
    setWindowFlags(Qt::MSWindowsFixedSizeDialogHint | windowFlags());
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    statusBar()->setSizeGripEnabled(false);

    // disable resize voodoo #2
    using namespace constants;
    const int hsize = font_size.height() * nrows;
    const int wsize = font_size.width() * ncols;
    setMinimumSize(wsize, hsize);
    setMaximumSize(wsize, hsize);
}

void text_window::paintEvent(QPaintEvent* e)
{
    QPainter painter(this);
    painter.setPen(QPen(Qt::NoBrush, 1, Qt::SolidLine, Qt::FlatCap));
    painter.fillRect(rect(), Qt::black);
    painter.setFont(monospace_font);

    for (int y = 0; y < nrows; y++)
        for (int x = 0; x < ncols; x++)
        {
            painter.save();
            draw_cell(&painter, world(x, y), x, y);
            painter.restore();
        }

    e->accept();
}

void text_window::draw_cell(QPainter* painter, const cell& c, int x, int y)
{
    painter->setPen(QPen(QColor(c.foreground.r, c.foreground.g, c.foreground.b, c.foreground.a), 1, Qt::SolidLine, Qt::FlatCap));
    const QRect bounds = font_metrics.boundingRect(c.symbol);
    const int x_ = -bounds.left() + (font_size.width() - bounds.width() + 1)/2;
    const int y_ = -bounds.top() + (font_size.height() - bounds.height() + 1)/2;
    if (c.background.a > 0)
        painter->fillRect(QRect(font_size.width()*x, font_size.height()*y,
                                font_size.width(), font_size.height()),
                          QColor(c.background.r, c.background.g, c.background.b));
    painter->drawText(font_size.width()*x + x_, font_size.height()*y + y_, QString(c.symbol));
}
