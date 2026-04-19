// SPDX-FileCopyrightText: 2026 AJ Ashton <aj@ajashton.ca>
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "mvt/MvtTypes.h"

#include <QString>
#include <optional>

namespace mvt {

struct DecodeResult {
    std::optional<Tile> tile;
    QString error;
};

DecodeResult decodeTile(const QByteArray& data);

} // namespace mvt
