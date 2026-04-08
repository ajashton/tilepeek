#pragma once

namespace WebMercator {

constexpr int TileSize = 256;

inline int mapSizePixels(int zoom)
{
    return TileSize * (1 << zoom);
}

double lonToPixelX(double lon, int zoom);
double latToPixelY(double lat, int zoom);
double pixelXToLon(double px, int zoom);
double pixelYToLat(double py, int zoom);

inline int pixelXToTileX(double px)
{
    return static_cast<int>(px) / TileSize;
}

inline int pixelYToTileY(double py)
{
    return static_cast<int>(py) / TileSize;
}

} // namespace WebMercator
