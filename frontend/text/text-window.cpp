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

text_window::text_window(fun_type f) :
//    monospace_font(QFont("Segoe UI Emoji", font_size)),
    monospace_font(QFont(QFontDatabase::systemFont(QFontDatabase::FixedFont).family(), font_size)),
    font_metrics(QFontMetrics(monospace_font)),
    sz(font_metrics.width("W")*3/2, font_metrics.lineSpacing()),
    functor(f)
{
    monospace_font.setStyleStrategy(QFont::PreferAntialias);

    // disable resize voodoo #1
    setWindowFlags(Qt::MSWindowsFixedSizeDialogHint | windowFlags());
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    statusBar()->setSizeGripEnabled(false);

    // disable resize voodoo #2
    using namespace constants;
    const int hsize = sz.height() * nrows;
    const int wsize = sz.width() * ncolumns;
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

    painter.save();
    functor(qt_impl::iter_cells(&painter, &font_metrics, sz));
    painter.restore();

    e->accept();
}
