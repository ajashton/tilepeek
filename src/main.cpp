#include "app/MainWindow.h"

#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("TilePeek");
    app.setApplicationVersion("0.1.0");

    MainWindow window;
    window.show();

    // Open file from first positional CLI argument
    const auto args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        if (!args[i].startsWith('-')) {
            window.openFile(args[i]);
            break;
        }
    }

    return app.exec();
}
