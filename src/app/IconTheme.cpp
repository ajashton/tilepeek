#include "app/IconTheme.h"

#include <QGuiApplication>
#include <QIcon>
#include <QString>
#include <QStringList>
#include <QStyleHints>

namespace tilepeek {

namespace {

QString themeNameForScheme(Qt::ColorScheme scheme)
{
    return scheme == Qt::ColorScheme::Dark ? QStringLiteral("breeze-dark")
                                           : QStringLiteral("breeze");
}

void applyThemeName(const QString& name)
{
#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
    // No system icon theme on these platforms — make our bundled theme primary.
    QIcon::setThemeName(name);
#else
    // On Linux, prefer the user's system theme; fall back to ours for any
    // icons it lacks.
    QIcon::setFallbackThemeName(name);
#endif
}

} // namespace

void installBundledIconThemes()
{
    auto paths = QIcon::themeSearchPaths();
    const QString resourcePath = QStringLiteral(":/icons");
    if (!paths.contains(resourcePath))
        paths.prepend(resourcePath);
    QIcon::setThemeSearchPaths(paths);

    auto* hints = QGuiApplication::styleHints();
    applyThemeName(themeNameForScheme(hints->colorScheme()));

    QObject::connect(hints, &QStyleHints::colorSchemeChanged,
                     qApp, [](Qt::ColorScheme scheme) {
                         applyThemeName(themeNameForScheme(scheme));
                     });
}

} // namespace tilepeek
