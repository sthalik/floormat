#pragma once

#include "color.hpp"

#include "range-v3.hpp"

#include <QPainter>
#include <QChar>
#include <QFontMetrics>
#include <QSize>

namespace window_position_impl {

using namespace ranges;

class window_cell
{
    QPainter* painter;
    QFontMetrics* m;
    QSize sz;

public:
    int x, y;

    window_cell();
    window_cell(QPainter* painter, QFontMetrics* m, int x, int y, const QSize& sz);
    void operator()(const color& c, const QString& k);
};

} // ns impl

using window_position_impl::window_cell;
