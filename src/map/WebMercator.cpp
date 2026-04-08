#include "map/WebMercator.h"

#include <cmath>
#include <numbers>

namespace WebMercator {

double lonToPixelX(double lon, int zoom)
{
    return (lon + 180.0) / 360.0 * mapSizePixels(zoom);
}

double latToPixelY(double lat, int zoom)
{
    double latRad = lat * std::numbers::pi / 180.0;
    double mercY = std::log(std::tan(std::numbers::pi / 4.0 + latRad / 2.0));
    return (1.0 - mercY / std::numbers::pi) / 2.0 * mapSizePixels(zoom);
}

double pixelXToLon(double px, int zoom)
{
    return px / mapSizePixels(zoom) * 360.0 - 180.0;
}

double pixelYToLat(double py, int zoom)
{
    double n = std::numbers::pi - 2.0 * std::numbers::pi * py / mapSizePixels(zoom);
    return 180.0 / std::numbers::pi * std::atan(std::sinh(n));
}

} // namespace WebMercator
