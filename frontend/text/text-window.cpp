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
    monospace_font(QFontDatabase::systemFont(QFontDatabase::FixedFont).family()),
    font_metrics(QFontMetrics(monospace_font)),
    font_size(font_metrics.width("W"), font_metrics.lineSpacing())
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
    //painter.setCompositionMode(QPainter::CompositionMode_Source);

    using constants::nrows;
    using constants::ncols;

    for (unsigned y = 0; y < nrows; y++)
        for (unsigned x = 0; x < ncols; x++)
        {
            painter.save();

            draw_cell(&painter, world(x, y), x, y);

            painter.restore();
        }

    //functor(&painter, &font_metrics, sz, qt_impl::iter_cells(constants::nrows, constants::ncols));


    e->accept();
}

void text_window::draw_cell(QPainter* painter, const cell& c, int x, int y)
{
    painter->save();
    painter->setPen(QPen(QColor(c.c.r, c.c.g, c.c.b, c.c.a), 1, Qt::SolidLine, Qt::FlatCap));
    const QRect bounds = font_metrics.boundingRect(c.symbol);
    const int x_ = -bounds.left() + (font_size.width() - bounds.width() + 1)/2;
    const int y_ = -bounds.top() + (font_size.height() - bounds.height() + 1)/2;
    painter->drawText(font_size.width()*x + x_, font_size.height()*y + y_, QString(c.symbol));
    painter->restore();
}
