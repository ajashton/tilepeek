#include "util/GzipUtils.h"

#include <zlib.h>

namespace GzipUtils {

bool isGzipCompressed(const QByteArray& data)
{
    return data.size() >= 2
        && static_cast<uint8_t>(data[0]) == 0x1f
        && static_cast<uint8_t>(data[1]) == 0x8b;
}

std::optional<QByteArray> decompress(const QByteArray& compressed)
{
    if (compressed.isEmpty())
        return std::nullopt;

    z_stream stream{};
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(compressed.data()));
    stream.avail_in = static_cast<uInt>(compressed.size());

    // 16 + MAX_WBITS tells zlib to expect gzip headers
    if (inflateInit2(&stream, 16 + MAX_WBITS) != Z_OK)
        return std::nullopt;

    QByteArray output;
    constexpr int ChunkSize = 32768;
    char buffer[ChunkSize];

    int ret;
    do {
        stream.next_out = reinterpret_cast<Bytef*>(buffer);
        stream.avail_out = ChunkSize;

        ret = inflate(&stream, Z_NO_FLUSH);
        if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
            inflateEnd(&stream);
            return std::nullopt;
        }

        int have = ChunkSize - static_cast<int>(stream.avail_out);
        output.append(buffer, have);
    } while (ret != Z_STREAM_END);

    inflateEnd(&stream);
    return output;
}

} // namespace GzipUtils
