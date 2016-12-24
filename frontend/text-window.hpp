#pragma once

#include "window-cell.hpp"
#include "constants.hpp"

#include "range-v3.hpp"

#include <functional>
#include <type_traits>

#include <QMainWindow>
#include <QFont>
#include <QFontMetrics>
#include <QSize>

struct qt_impl final
{
    // CAVEAT defining as static function outside class leads to link errors
    static auto iter_cells(QPainter* painter, QFontMetrics* m, const QSize& sz)
    {
        using namespace constants;
        using namespace ranges;
        // CAVEAT using capture by reference leads to use-after-free of the i, j indices
        return view::ints(0, nrows) >>= [=](int i) { return
                    view::ints(0, ncolumns) >>= [=](int j) { return
                        yield(window_cell(painter, m, j, i, sz));
            };
        };
    }

    qt_impl() = delete;
};

class text_window : public QMainWindow
{
    Q_OBJECT

    QFont monospace_font;
    QFontMetrics font_metrics;
    const QSize sz;

    void paintEvent(QPaintEvent* e) override;

public:
    using sequence_type = decltype(qt_impl::iter_cells(nullptr, nullptr, QSize()));
    using fun_type = std::function<void(sequence_type)>;

    fun_type functor;
    text_window(fun_type f);
};
