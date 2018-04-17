#include "text-window.hpp"
#include "color.hpp"
#include "cell.hpp"
#include "util.hpp"

#include <QApplication>
#include <QDir>
#include <QTextCodec>
#include <QDebug>

#include <windows.h>

#include "world/static.hpp"

using namespace constants;

extern void gen();

// workaround QTBUG-38598, allow for launching from another directory
static void add_program_library_path()
{
#if defined _WIN32
    // Windows 10 allows for paths longer than MAX_PATH via fsutil and friends, shit
    const char* p = _pgmptr;
    char path[4096+1] {};

    strncpy(path, p, sizeof(path)-1);
    path[sizeof(path)-1] = '\0';

    char* ptr = strrchr(path, '\\');
    if (ptr)
    {
        *ptr = '\0';
        QCoreApplication::setLibraryPaths({ path });
    }
#endif
}

extern "C" int main(int argc, char** argv)
{
#if defined _WIN32
    (void) AttachConsole(ATTACH_PARENT_PROCESS);
#endif
    add_program_library_path();

    for (int x = 0; x < 8; x++)
        for (int y = 0; y < 8; y++)
            qDebug() << x << y << "=>" << world::hilbert_lookup_2d_to_1d({x, y});

    return 0;

#if 0
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));

    QApplication app(argc, argv);
    (void) QDir::setCurrent(QCoreApplication::applicationDirPath());

    text_window w;
    w.show();

    app.exec();
#else
    gen();
#endif

    fflush(stdout);
    fflush(stderr);

    return 0;
}

#if defined(Q_CREATOR_RUN)
#   pragma clang diagnostic ignored "-Wmain"
#endif

#ifdef _WIN32
int WINAPI
WinMain (struct HINSTANCE__*, struct HINSTANCE__*, char*, int)
{
  return main (__argc, __argv);
}
#endif
