#include "stats/TileStatsWorker.h"
#include "mbtiles/MBTilesReader.h"

#include <QSqlQuery>
#include <algorithm>
#include <cmath>
#include <vector>

static int64_t percentile(const std::vector<int64_t>& sorted, double p)
{
    if (sorted.empty())
        return 0;
    int index = static_cast<int>(std::ceil(p / 100.0 * sorted.size())) - 1;
    index = std::clamp(index, 0, static_cast<int>(sorted.size()) - 1);
    return sorted[index];
}

static ZoomLevelStats computeStats(const std::vector<int64_t>& sortedSizes)
{
    ZoomLevelStats stats;
    stats.tileCount = static_cast<int>(sortedSizes.size());
    stats.p50Size = percentile(sortedSizes, 50.0);
    stats.p90Size = percentile(sortedSizes, 90.0);
    stats.p99Size = percentile(sortedSizes, 99.0);
    return stats;
}

TileStatsWorker::TileStatsWorker(const QString& filePath, QObject* parent)
    : QObject(parent)
    , m_filePath(filePath)
{
}

void TileStatsWorker::calculate()
{
    TileStatistics result;

    MBTilesReader reader(m_filePath);
    if (!reader.open()) {
        emit finished(result);
        return;
    }

    // Query sizes ordered by zoom level and size for efficient processing
    QSqlQuery query(QSqlDatabase::database(reader.connectionName()));
    if (!query.exec("SELECT zoom_level, LENGTH(tile_data) FROM tiles "
                    "ORDER BY zoom_level, LENGTH(tile_data)")) {
        emit finished(result);
        return;
    }

    QMap<int, std::vector<int64_t>> perZoomSizes;
    std::vector<int64_t> allSizes;

    while (query.next()) {
        int zoom = query.value(0).toInt();
        int64_t size = query.value(1).toLongLong();
        perZoomSizes[zoom].push_back(size);
        allSizes.push_back(size);
    }

    // Per-zoom stats (already sorted due to ORDER BY)
    for (auto it = perZoomSizes.constBegin(); it != perZoomSizes.constEnd(); ++it)
        result.perZoom[it.key()] = computeStats(it.value());

    // Total stats (need to sort the combined vector)
    std::ranges::sort(allSizes);
    result.total = computeStats(allSizes);

    emit finished(result);
}
