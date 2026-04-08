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

    virtual int minZoom() const = 0;
    virtual int maxZoom() const = 0;
};
