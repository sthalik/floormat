#pragma once

#include "color.hpp"

#include <QChar>

struct cell final
{
    QChar symbol;
    const color foreground, background;

    cell(QChar symbol, color foreground, color background = color::from_rgb(0, 0, 0, 0))
        : symbol(symbol), foreground(foreground), background(background)
    {}

    cell() : symbol(0), foreground(color::from_rgb(0, 0, 0, 0)), background(color::from_rgb(0, 0, 0, 0)) {}
};
