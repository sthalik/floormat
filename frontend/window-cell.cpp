#include "window-cell.hpp"
#include "constants.hpp"

#include <QChar>
#include <QColor>
#include <QFont>

#include <QDebug>

using namespace window_position_impl;

void window_cell::operator()(const color& c, const QString& k)
{
    using namespace constants;

    if (painter)
    {
        painter->save();
        painter->setPen(QPen(QColor(c.r, c.g, c.b, c.a), 1, Qt::SolidLine, Qt::FlatCap));
        const QRect bounds = m->boundingRect(k);
        const int x_ = -bounds.left() + (sz.width() - bounds.width() + 1)/2;
        const int y_ = -bounds.top() + (sz.height() - bounds.height() + 1)/2;
        painter->drawText(sz.width()*x + x_, sz.height()*y + y_, k);
        painter->restore();
    }
}

window_cell::window_cell(QPainter* painter, QFontMetrics* m, int x, int y, const QSize& sz) :
    painter(painter), m(m),
    sz(sz),
    x(x), y(y)
{
}

window_cell::window_cell() :
    painter(nullptr), m(nullptr),
    x(-1), y(-1)
{}
