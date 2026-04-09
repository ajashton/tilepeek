#pragma once

#include <QMap>
#include <QMetaType>
#include <cstdint>

struct ZoomLevelStats {
    int tileCount = 0;
    int64_t p50Size = 0;
    int64_t p90Size = 0;
    int64_t p99Size = 0;
    int64_t maxSize = 0;
};

struct TileStatistics {
    ZoomLevelStats total;
    QMap<int, ZoomLevelStats> perZoom;
};

Q_DECLARE_METATYPE(TileStatistics)
