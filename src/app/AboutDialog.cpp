// SPDX-FileCopyrightText: 2026 AJ Ashton <aj@ajashton.ca>
// SPDX-License-Identifier: GPL-3.0-only

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

QFrame* AboutDialog::makeDivider(QWidget* parent)
{
    auto* line = new QFrame(parent);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    return line;
}

QString AboutDialog::qtVersionLine()
{
    if (QLibraryInfo::isSharedBuild()) {
        return tr("Built with Qt %1 (running %2)")
            .arg(QT_VERSION_STR, qVersion());
    }
    return tr("Built with Qt %1 (static)").arg(QT_VERSION_STR);
}

QWidget* AboutDialog::buildAboutTab(QWidget* parent)
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

    //: Heading shown on the About tab. The <b> markup must be preserved; the
    //: name itself is a brand and is normally kept as "TilePeek" in Latin
    //: scripts (transliterate for other scripts if appropriate).
    auto* nameLabel = new QLabel(tr("<b style='font-size:16pt'>TilePeek</b>"));
    nameLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(nameLabel);

    auto* versionLabel = new QLabel(tr("Version %1").arg(TILEPEEK_VERSION));
    versionLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(versionLabel);

    //: Copyright line on the About tab. Preserve the <a href="..."> link.
    auto* copyrightLabel = new QLabel(tr(
        "\u00A9 2026 AJ Ashton \u2013 <a href=\"https://ajashton.ca/\">ajashton.ca</a>"));
    copyrightLabel->setAlignment(Qt::AlignCenter);
    copyrightLabel->setOpenExternalLinks(true);
    layout->addWidget(copyrightLabel);

    //: Project link line on the About tab. Preserve the <br> and the
    //: <a href="..."> markup and URL.
    auto* linkLabel = new QLabel(tr(
        "Get the source code, report bugs, or request features at<br>"
        "<a href=\"https://github.com/ajashton/tilepeek/\">github.com/ajashton/tilepeek</a>"));
    linkLabel->setAlignment(Qt::AlignCenter);
    linkLabel->setWordWrap(true);
    linkLabel->setOpenExternalLinks(true);
    layout->addWidget(linkLabel);

    layout->addWidget(makeDivider(tab));

    //: Third-party attribution block on the About tab. Preserve all <br>,
    //: <a href="..."> markup, URLs, license names (GNU LGPLv3, CC-BY 4.0),
    //: and proper names (Protomaps, Peter Kovesi, CET-C6, Breeze, Uri
    //: Herrera, KDE). Translate only the descriptive prose.
    auto* attribLabel = new QLabel(tr(
        "PMTiles parsing code \u00A9 2021 and later<br>"
        "Protomaps LLC and contributors \u2013 <a href=\"https://protomaps.com/\">protomaps.com</a><br>"
        "<br>"
        "Vector layers color palette based on \u201CCET-C6\u201D<br>"
        "by Peter Kovesi, <a href=\"https://creativecommons.org/licenses/by/4.0/\">CC-BY 4.0</a> \u2013 <a href=\"https://colorcet.com\">colorcet.com</a><br>"
        "<br>"
        "Breeze icons \u00A9 2014 <a href=\"mailto:uri_herrera@nitrux.in\">Uri Herrera</a> and others,<br>"
        "<a href=\"https://www.gnu.org/licenses/lgpl-3.0.html\">GNU LGPLv3</a> \u2013 <a href=\"https://invent.kde.org/frameworks/breeze-icons\">invent.kde.org/frameworks/breeze-icons</a>"));
    attribLabel->setWordWrap(true);
    attribLabel->setOpenExternalLinks(true);
    attribLabel->setAlignment(Qt::AlignCenter);
    auto attribFont = attribLabel->font();
    attribLabel->setFont(attribFont);
    layout->addWidget(attribLabel);

    layout->addStretch();
    return tab;
}

QWidget* AboutDialog::buildLicenseTab(QWidget* parent)
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
        view->setPlainText(tr("License text could not be loaded."));
    }
    auto cursor = view->textCursor();
    cursor.movePosition(QTextCursor::Start);
    view->setTextCursor(cursor);

    layout->addWidget(view);
    return tab;
}

QWidget* AboutDialog::buildSystemInfoTab(QWidget* parent)
{
    auto* tab = new QWidget(parent);
    auto* layout = new QVBoxLayout(tab);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* view = new QPlainTextEdit(tab);
    view->setReadOnly(true);
    view->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    view->setLineWrapMode(QPlainTextEdit::NoWrap);

    QString text;
    text += tr("TilePeek version: %1").arg(TILEPEEK_VERSION) + QLatin1Char('\n');
    text += tr("Git SHA: %1").arg(TILEPEEK_GIT_SHA) + QLatin1Char('\n');
    text += qtVersionLine() + QLatin1Char('\n');
    text += tr("Operating system: %1").arg(QSysInfo::prettyProductName());
    view->setPlainText(text);

    layout->addWidget(view);
    return tab;
}

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("About TilePeek"));
    resize(580, 480);
    setMinimumSize(420, 360);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(8);

    auto* tabs = new QTabWidget(this);
    tabs->addTab(buildAboutTab(tabs), tr("About"));
    tabs->addTab(buildLicenseTab(tabs), tr("License"));
    tabs->addTab(buildSystemInfoTab(tabs), tr("System Info"));
    layout->addWidget(tabs);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttonBox);
}
