// SPDX-FileCopyrightText: 2026 AJ Ashton <aj@ajashton.ca>
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QByteArray>
#include <optional>

namespace GzipUtils {

bool isGzipCompressed(const QByteArray& data);
std::optional<QByteArray> decompress(const QByteArray& compressed);

} // namespace GzipUtils
