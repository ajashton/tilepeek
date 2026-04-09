#pragma once

#include "mvt/MvtTypes.h"

#include <QColor>
#include <QPixmap>
#include <QSet>
#include <QString>
#include <unordered_map>

class VectorTileRenderer {
public:
    static QPixmap render(const mvt::Tile& tile,
                          const std::unordered_map<std::string, QColor>& layerColors,
                          const QSet<QString>& hiddenLayers,
                          int tileSize = 256);
};
