// SPDX-FileCopyrightText: 2026 AJ Ashton <aj@ajashton.ca>
// SPDX-License-Identifier: GPL-3.0-only

#include "util/TileCoords.h"

namespace TileCoords {

int tmsToXyz(int zoom, int tmsRow)
{
    return (1 << zoom) - 1 - tmsRow;
}

int xyzToTms(int zoom, int xyzY)
{
    return (1 << zoom) - 1 - xyzY;
}

} // namespace TileCoords
