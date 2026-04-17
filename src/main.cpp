#include "app/MainWindow.h"
#include "version.h"

#include <QApplication>
#include <QFileOpenEvent>
#include <QString>

namespace {

class TilePeekApplication : public QApplication {
public:
    using QApplication::QApplication;

    void setMainWindow(MainWindow* window)
    {
        m_window = window;
        if (!m_pendingFile.isEmpty()) {
            m_window->openFile(m_pendingFile);
            m_pendingFile.clear();
        }
    }

    bool event(QEvent* e) override
    {
        if (e->type() == QEvent::FileOpen) {
            const QString path = static_cast<QFileOpenEvent*>(e)->file();
            if (m_window) {
                m_window->openFile(path);
            } else {
                m_pendingFile = path;
            }
            return true;
        }
        return QApplication::event(e);
    }

private:
    MainWindow* m_window = nullptr;
    QString m_pendingFile;
};

} // namespace

int main(int argc, char* argv[])
{
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    TilePeekApplication app(argc, argv);
    app.setApplicationName("TilePeek");
    app.setApplicationDisplayName("TilePeek");
    app.setOrganizationDomain("tilepeek.com");
    app.setApplicationVersion(TILEPEEK_VERSION);
    app.setDesktopFileName("com.tilepeek.TilePeek");

    MainWindow window;
    window.show();
    app.setMainWindow(&window);

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
