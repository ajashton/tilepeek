// SPDX-FileCopyrightText: 2026 AJ Ashton <aj@ajashton.ca>
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "map/UnclippedTileResult.h"
#include "mvt/MvtTypes.h"

#include <QColor>
#include <QImage>
#include <QSet>
#include <QString>
#include <unordered_map>

class VectorTileRenderer {
public:
    static QImage render(const mvt::Tile& tile,
                         const std::unordered_map<std::string, QColor>& layerColors,
                         const QSet<QString>& hiddenLayers,
                         int tileSize = 256,
                         qreal dpr = 1.0);

    static UnclippedTileResult renderUnclipped(const mvt::Tile& tile,
                                               const std::unordered_map<std::string, QColor>& layerColors,
                                               const QSet<QString>& hiddenLayers,
                                               int tileSize = 256,
                                               qreal dpr = 1.0);
};
