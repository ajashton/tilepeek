#pragma once

#include <QByteArray>
#include <optional>

class TileSource {
public:
    virtual ~TileSource() = default;

    // Read tile data for XYZ coordinates (y=0 at north).
    // Returns std::nullopt if the tile doesn't exist.
    virtual std::optional<QByteArray> readTile(int zoom, int x, int y) = 0;

    // Return the on-disk size of a tile in bytes (compressed size if applicable).
    // Default implementation reads the full tile; subclasses may override for efficiency.
    virtual std::optional<int> tileSize(int zoom, int x, int y)
    {
        auto data = readTile(zoom, x, y);
        return data ? std::optional(static_cast<int>(data->size())) : std::nullopt;
    }
};
