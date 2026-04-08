#include "util/FormatUtils.h"

namespace FormatUtils {

QString formatTileSize(int bytes)
{
    if (bytes < 1024)
        return QString("%1 B").arg(bytes);
    double kib = bytes / 1024.0;
    return QString("%1 KiB").arg(kib, 0, 'f', 1);
}

} // namespace FormatUtils
