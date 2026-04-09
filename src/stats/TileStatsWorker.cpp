#include "stats/TileStatsWorker.h"
#include "mbtiles/MBTilesReader.h"
#include "pmtiles/PMTilesReader.h"
#include "util/GzipUtils.h"

#include <QSqlQuery>
#include <algorithm>
#include <cmath>
#include <vector>
#include <pmtiles.hpp>

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

    if (m_filePath.endsWith(".pmtiles", Qt::CaseInsensitive))
        calculatePMTiles(result);
    else
        calculateMBTiles(result);

    emit finished(result);
}

void TileStatsWorker::calculateMBTiles(TileStatistics& result)
{
    MBTilesReader reader(m_filePath);
    if (!reader.open())
        return;

    QSqlQuery query(QSqlDatabase::database(reader.connectionName()));
    if (!query.exec("SELECT zoom_level, LENGTH(tile_data) FROM tiles "
                    "ORDER BY zoom_level, LENGTH(tile_data)"))
        return;

    QMap<int, std::vector<int64_t>> perZoomSizes;
    std::vector<int64_t> allSizes;

    while (query.next()) {
        int zoom = query.value(0).toInt();
        int64_t size = query.value(1).toLongLong();
        perZoomSizes[zoom].push_back(size);
        allSizes.push_back(size);
    }

    for (auto it = perZoomSizes.constBegin(); it != perZoomSizes.constEnd(); ++it)
        result.perZoom[it.key()] = computeStats(it.value());

    std::ranges::sort(allSizes);
    result.total = computeStats(allSizes);
}

void TileStatsWorker::calculatePMTiles(TileStatistics& result)
{
    PMTilesReader reader(m_filePath);
    if (!reader.open())
        return;

    auto decompress = [](const std::string& data, uint8_t compression) -> std::string {
        if (compression == pmtiles::COMPRESSION_NONE
            || compression == pmtiles::COMPRESSION_UNKNOWN)
            return data;
        if (compression == pmtiles::COMPRESSION_GZIP) {
            QByteArray input(data.data(), static_cast<int>(data.size()));
            auto out = GzipUtils::decompress(input);
            if (!out)
                throw std::runtime_error("gzip decompression failed");
            return std::string(out->data(), out->size());
        }
        throw std::runtime_error("unsupported compression");
    };

    // Memory-mapped data is available through the reader's internal state,
    // but we need raw access for entries_tms. Open the file directly.
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly))
        return;
    auto* mapped = file.map(0, file.size());
    if (!mapped)
        return;
    auto* pmtilesMap = reinterpret_cast<const char*>(mapped);

    try {
        auto entries = pmtiles::entries_tms(decompress, pmtilesMap);

        QMap<int, std::vector<int64_t>> perZoomSizes;
        std::vector<int64_t> allSizes;

        for (const auto& entry : entries) {
            int zoom = entry.z;
            int64_t size = entry.length;
            perZoomSizes[zoom].push_back(size);
            allSizes.push_back(size);
        }

        for (auto it = perZoomSizes.begin(); it != perZoomSizes.end(); ++it) {
            std::ranges::sort(it.value());
            result.perZoom[it.key()] = computeStats(it.value());
        }

        std::ranges::sort(allSizes);
        result.total = computeStats(allSizes);
    } catch (const std::exception&) {
        // Stats calculation failed — return empty result
    }
}
