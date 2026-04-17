#pragma once

#include "map/TileProvider.h"
#include "map/TileSource.h"

#include <QMutex>
#include <QString>
#include <memory>

struct FormatValidationResult {
    enum class Status { Ok, FormatMismatch, UnrecognizedFormat, Unsupported };
    Status status;
    QString message;
};

class RasterTileProvider : public TileProvider {
public:
    RasterTileProvider(std::unique_ptr<TileSource> source,
                       const QString& formatHint,
                       int minZoom, int maxZoom);

    // Thread-safe: source access is serialized internally; QImage decoding
    // happens on whatever thread calls this.
    std::optional<QImage> tileAt(int zoom, int x, int y) override;
    std::optional<int> tileSizeAt(int zoom, int x, int y) override;
    int minZoom() const override { return m_minZoom; }
    int maxZoom() const override { return m_maxZoom; }

    FormatValidationResult validateFormat();
    int detectNativeTileSize() const;

private:
    std::unique_ptr<TileSource> m_source;
    QString m_formatHint;
    int m_minZoom;
    int m_maxZoom;
    mutable QMutex m_sourceMutex;
};
