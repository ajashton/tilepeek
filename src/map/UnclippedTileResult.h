#pragma once

#include <QImage>

struct UnclippedTileResult {
    QImage image;
    double bufferRatio = 0.0; // ratio of buffer to extent on each side
};
