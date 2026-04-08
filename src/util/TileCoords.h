#pragma once

namespace TileCoords {

// Convert TMS tile_row (y=0 at southwest) to XYZ y (y=0 at northwest)
// Formula: y = (2^zoom - 1) - tmsRow
int tmsToXyz(int zoom, int tmsRow);

// Convert XYZ y back to TMS tile_row (same formula, it's self-inverse)
int xyzToTms(int zoom, int xyzY);

} // namespace TileCoords
