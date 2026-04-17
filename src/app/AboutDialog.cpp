#include "AboutDialog.h"
#include "version.h"

#include <QDialogButtonBox>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QFrame>
#include <QIcon>
#include <QLabel>
#include <QLibraryInfo>
#include <QPlainTextEdit>
#include <QSysInfo>
#include <QTabWidget>
#include <QTextStream>
#include <QVBoxLayout>
#include <QtGlobal>

namespace {

QFrame* makeDivider(QWidget* parent)
{
    auto* line = new QFrame(parent);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    return line;
}

QString qtVersionLine()
{
    if (QLibraryInfo::isSharedBuild()) {
        return QString("Built with Qt %1 (running %2)")
            .arg(QT_VERSION_STR, qVersion());
    }
    return QString("Built with Qt %1 (static)").arg(QT_VERSION_STR);
}

QWidget* buildAboutTab(QWidget* parent)
{
    auto* tab = new QWidget(parent);
    auto* layout = new QVBoxLayout(tab);
    layout->setSpacing(8);

    auto icon = QIcon::fromTheme("com.tilepeek.TilePeek");
    if (icon.isNull())
        icon = QIcon(":/icons/tilepeek.svg");
    auto* iconLabel = new QLabel;
    iconLabel->setPixmap(icon.pixmap(64, 64));
    iconLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(iconLabel);

    auto* nameLabel = new QLabel("<b style='font-size:16pt'>TilePeek</b>");
    nameLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(nameLabel);

    auto* versionLabel = new QLabel(QString("Version %1").arg(TILEPEEK_VERSION));
    versionLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(versionLabel);

    layout->addWidget(makeDivider(tab));

    auto* copyrightLabel = new QLabel("\u00A9 2026 AJ Ashton");
    copyrightLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(copyrightLabel);

    auto* attribLabel = new QLabel(
        "PMTiles parsing code \u00A9 2021 and later<br>"
        "Protomaps LLC and contributors<br>"
        "<br>"
        "Vector layers color palette based on \u201CCET-C6\u201D<br>"
        "by Peter Kovesi, CC-BY 4.0 \u2013 <a href=\"https://colorcet.com\">colorcet.com</a>");
    attribLabel->setWordWrap(true);
    attribLabel->setOpenExternalLinks(true);
    attribLabel->setAlignment(Qt::AlignCenter);
    auto attribFont = attribLabel->font();
    attribLabel->setFont(attribFont);
    layout->addWidget(attribLabel);

    layout->addWidget(makeDivider(tab));

    auto* linkLabel = new QLabel(
        "Get the source code, report bugs, or request features at<br>"
        "<a href=\"https://github.com/ajashton/tilepeek/\">github.com/ajashton/tilepeek</a>");
    linkLabel->setAlignment(Qt::AlignCenter);
    linkLabel->setWordWrap(true);
    linkLabel->setOpenExternalLinks(true);
    layout->addWidget(linkLabel);

    layout->addStretch();
    return tab;
}

QWidget* buildLicenseTab(QWidget* parent)
{
    auto* tab = new QWidget(parent);
    auto* layout = new QVBoxLayout(tab);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* view = new QPlainTextEdit(tab);
    view->setReadOnly(true);
    view->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    view->setLineWrapMode(QPlainTextEdit::NoWrap);

    QFile file(":/text/LICENSE");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        view->setPlainText(stream.readAll());
    } else {
        view->setPlainText("License text could not be loaded.");
    }
    auto cursor = view->textCursor();
    cursor.movePosition(QTextCursor::Start);
    view->setTextCursor(cursor);

    layout->addWidget(view);
    return tab;
}

QWidget* buildSystemInfoTab(QWidget* parent)
{
    auto* tab = new QWidget(parent);
    auto* layout = new QVBoxLayout(tab);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* view = new QPlainTextEdit(tab);
    view->setReadOnly(true);
    view->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    view->setLineWrapMode(QPlainTextEdit::NoWrap);

    QString text;
    text += QString("TilePeek version: %1\n").arg(TILEPEEK_VERSION);
    text += QString("Git SHA: %1\n").arg(TILEPEEK_GIT_SHA);
    text += qtVersionLine() + "\n";
    text += QString("Operating system: %1").arg(QSysInfo::prettyProductName());
    view->setPlainText(text);

    layout->addWidget(view);
    return tab;
}

} // namespace

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("About TilePeek");
    resize(580, 480);
    setMinimumSize(420, 360);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(8);

    auto* tabs = new QTabWidget(this);
    tabs->addTab(buildAboutTab(tabs), "About");
    tabs->addTab(buildLicenseTab(tabs), "License");
    tabs->addTab(buildSystemInfoTab(tabs), "System Info");
    layout->addWidget(tabs);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttonBox);
}
