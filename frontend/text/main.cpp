#include "text-window.hpp"
#include "color.hpp"
#include "util.hpp"
#include "window-cell.hpp"

#include <QApplication>
#include <QDir>
#include <QTextCodec>

using namespace ranges;
using namespace constants;

#if 1
static void fun(text_window::sequence_type f)
{
    // this is meant to look like a tree but qt doesn't render quite right.
    // might as well skip all the emoji fanciness later.
    static constexpr std::uint8_t ints[] = { 0xf0, 0x9f, 0x8c, 0xb2 };
    QString c = QString::fromUtf8(reinterpret_cast<const char*>(ints));
    int k = 0;

    using namespace ranges;

    RANGES_FOR(window_cell cell, f)
    {
        cell(color::from_rgb(255, 255, 255, 255), (cell.x + cell.y) % 2 ? " " : c);
    }
}
#endif

extern "C" int main(int argc, char** argv)
{
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));

    QApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
    QApplication app(argc, argv);

    (void) QDir::setCurrent(QCoreApplication::applicationDirPath());

    //color c = hsl(300., .5, .5);
    //qDebug() << c.r << c.g << c.b;

    text_window w(&fun);
    w.show();

    app.exec();

    return 0;
}
