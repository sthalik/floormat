#pragma once

#include "color.hpp"

#include <QChar>

struct cell final
{
    QChar symbol;
    const color c;

    cell(QChar symbol, color c) : symbol(symbol), c(c) {}

    cell() : symbol(0), c(color::from_rgb(0, 0, 0, 0)) {}
};
