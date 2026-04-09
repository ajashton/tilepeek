#pragma once

#include "map/TileProvider.h"
#include "mbtiles/MBTilesReader.h"

#include <QString>
#include <memory>

struct FormatValidationResult {
    enum class Status { Ok, FormatMismatch, UnrecognizedFormat, Unsupported };
    Status status;
    QString message;
};

class RasterTileProvider : public TileProvider {
public:
    RasterTileProvider(std::unique_ptr<MBTilesReader> reader,
                       const QString& formatHint,
                       int minZoom, int maxZoom);

    std::optional<QPixmap> tileAt(int zoom, int x, int y) override;
    std::optional<int> tileSizeAt(int zoom, int x, int y) override;
    int minZoom() const override { return m_minZoom; }
    int maxZoom() const override { return m_maxZoom; }

    FormatValidationResult validateFormat();
    int detectNativeTileSize() const;

private:
    std::unique_ptr<MBTilesReader> m_reader;
    QString m_formatHint;
    int m_minZoom;
    int m_maxZoom;
};
