#pragma once

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

    // Render a tile at a specific pixel size. Default returns the standard tile.
    // VectorTileProvider overrides this to re-render geometry at the requested size.
    virtual std::optional<QPixmap> tileAtSize(int zoom, int x, int y, int /*size*/)
    {
        return tileAt(zoom, x, y);
    }

    virtual int minZoom() const = 0;
    virtual int maxZoom() const = 0;
};
