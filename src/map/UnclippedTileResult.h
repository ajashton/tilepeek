#pragma once

#include <QPixmap>

struct UnclippedTileResult {
    QPixmap pixmap;
    double bufferRatio = 0.0; // ratio of buffer to extent on each side
};
