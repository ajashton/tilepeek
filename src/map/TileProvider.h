#pragma once

#include "map/UnclippedTileResult.h"

#include <QPixmap>
#include <optional>

class TileProvider {
public:
    virtual ~TileProvider() = default;

    // Fetch a decoded tile image for the given XYZ coordinates.
    // Returns std::nullopt if the tile doesn't exist or can't be decoded.
    // Coordinates use XYZ convention (y=0 at north).
    virtual std::optional<QPixmap> tileAt(int zoom, int x, int y) = 0;
    virtual std::optional<int> tileSizeAt(int zoom, int x, int y) = 0;

    // Render a tile at a specific logical pixel size. Default returns the standard tile.
    // VectorTileProvider overrides this to re-render geometry at the requested size.
    virtual std::optional<QPixmap> tileAtSize(int zoom, int x, int y, int /*size*/)
    {
        return tileAt(zoom, x, y);
    }

    // Render a tile without clipping, showing buffer area data. The size parameter
    // is in logical pixels. Only meaningful for vector tiles; default returns nullopt.
    virtual std::optional<UnclippedTileResult> tileUnclipped(int, int, int, int)
    {
        return std::nullopt;
    }

    // Set the device pixel ratio to use when rasterizing tiles. Providers that
    // rasterize from source data (e.g. vector) honor this to produce crisp output
    // on HiDPI displays; raster providers ignore it.
    virtual void setDevicePixelRatio(qreal /*dpr*/) {}

    virtual int minZoom() const = 0;
    virtual int maxZoom() const = 0;
};
