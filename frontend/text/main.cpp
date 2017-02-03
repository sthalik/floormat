#include "text-window.hpp"
#include "color.hpp"
#include "cell.hpp"
#include "util.hpp"

#include <QApplication>
#include <QDir>
#include <QTextCodec>

using namespace constants;

template<typename seq>
static void fun()
{
}

extern void gen();

extern "C" int main(int argc, char** argv)
{
#if 1
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));

    QApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
    QApplication app(argc, argv);

    (void) QDir::setCurrent(QCoreApplication::applicationDirPath());

    text_window w;
    w.show();

    app.exec();
#else
    gen();
#endif
    return 0;
}
