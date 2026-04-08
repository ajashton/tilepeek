#pragma once

#include <QByteArray>
#include <optional>

namespace GzipUtils {

bool isGzipCompressed(const QByteArray& data);
std::optional<QByteArray> decompress(const QByteArray& compressed);

} // namespace GzipUtils
