// SPDX-FileCopyrightText: 2026 AJ Ashton <aj@ajashton.ca>
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QImage>

struct UnclippedTileResult {
    QImage image;
    double bufferRatio = 0.0; // ratio of buffer to extent on each side
};
