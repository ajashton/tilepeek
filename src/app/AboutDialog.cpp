#include "AboutDialog.h"
#include "version.h"

#include <QDialogButtonBox>
#include <QIcon>
#include <QLabel>
#include <QVBoxLayout>

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("About TilePeek");
    setFixedWidth(420);
    setSizeGripEnabled(false);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(8);

    // Icon
    auto icon = QIcon::fromTheme("com.tilepeek.TilePeek");
    if (icon.isNull())
        icon = QIcon(":/icons/tilepeek.svg");
    auto* iconLabel = new QLabel;
    iconLabel->setPixmap(icon.pixmap(64, 64));
    iconLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(iconLabel);

    // App name
    auto* nameLabel = new QLabel("<b style='font-size:16pt'>TilePeek</b>");
    nameLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(nameLabel);

    // Version
    auto* versionLabel = new QLabel(QString("Version %1").arg(TILEPEEK_VERSION));
    versionLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(versionLabel);

    // Copyright
    auto* copyrightLabel = new QLabel("\u00A9 2026 AJ Ashton");
    copyrightLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(copyrightLabel);

    // GitHub link
    auto* linkLabel = new QLabel(
        "<a href=\"https://github.com/ajashton/tilepeek/\">github.com/ajashton/tilepeek</a>");
    linkLabel->setAlignment(Qt::AlignCenter);
    linkLabel->setOpenExternalLinks(true);
    layout->addWidget(linkLabel);

    // Qt version
    auto* qtLabel = new QLabel(QString("Built with Qt %1").arg(QT_VERSION_STR));
    qtLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(qtLabel);

    layout->addSpacing(8);

    // Attributions
    auto* attribLabel = new QLabel(
        "Vector layers color palette based on \u201CCET-C6\u201D by Peter Kovesi, "
        "CC-BY 4.0 \u2013 <a href=\"https://colorcet.com\">colorcet.com</a>"
        "<br><br>"
        "PMTiles parsing code \u00A9 2021 and later, Protomaps LLC and contributors");
    attribLabel->setWordWrap(true);
    attribLabel->setOpenExternalLinks(true);
    attribLabel->setAlignment(Qt::AlignCenter);
    auto attribFont = attribLabel->font();
    attribFont.setPointSizeF(attribFont.pointSizeF() * 0.9);
    attribLabel->setFont(attribFont);
    layout->addWidget(attribLabel);

    layout->addStretch();

    // Close button
    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttonBox);
}
