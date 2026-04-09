#include "pmtiles/PMTilesReader.h"
#include "util/GzipUtils.h"

#include <stdexcept>

PMTilesReader::PMTilesReader(const QString& filePath)
    : m_filePath(filePath)
    , m_file(filePath)
{
}

PMTilesReader::~PMTilesReader()
{
    close();
}

bool PMTilesReader::open()
{
    if (!m_file.open(QIODevice::ReadOnly))
        return false;

    m_fileSize = m_file.size();
    if (m_fileSize < 127)
        return false;

    auto* mapped = m_file.map(0, m_fileSize);
    if (!mapped)
        return false;

    m_mappedData = reinterpret_cast<const char*>(mapped);

    try {
        std::string headerBytes(m_mappedData, 127);
        m_header = pmtiles::deserialize_header(headerBytes);
    } catch (const std::exception&) {
        close();
        return false;
    }

    return true;
}

void PMTilesReader::close()
{
    if (m_mappedData) {
        m_file.unmap(reinterpret_cast<uchar*>(const_cast<char*>(m_mappedData)));
        m_mappedData = nullptr;
    }
    if (m_file.isOpen())
        m_file.close();
}

PMTilesValidationResult PMTilesReader::validate() const
{
    PMTilesValidationResult result;

    if (m_header.internal_compression == pmtiles::COMPRESSION_BROTLI) {
        result.errors.append("Brotli compression is not supported");
        return result;
    }
    if (m_header.internal_compression == pmtiles::COMPRESSION_ZSTD) {
        result.errors.append("Zstandard compression is not supported");
        return result;
    }
    if (m_header.tile_compression == pmtiles::COMPRESSION_BROTLI) {
        result.errors.append("Brotli tile compression is not supported");
        return result;
    }
    if (m_header.tile_compression == pmtiles::COMPRESSION_ZSTD) {
        result.errors.append("Zstandard tile compression is not supported");
        return result;
    }

    result.valid = true;
    return result;
}

QString PMTilesReader::readJsonMetadata() const
{
    if (!m_mappedData || m_header.json_metadata_bytes == 0)
        return {};

    uint64_t offset = m_header.json_metadata_offset;
    uint64_t length = m_header.json_metadata_bytes;

    if (offset + length > static_cast<uint64_t>(m_fileSize))
        return {};

    std::string raw(m_mappedData + offset, static_cast<size_t>(length));

    try {
        std::string decompressed = decompress(raw, m_header.internal_compression);
        return QString::fromUtf8(decompressed.data(), static_cast<int>(decompressed.size()));
    } catch (const std::exception&) {
        return {};
    }
}

std::optional<QByteArray> PMTilesReader::readTile(int zoom, int x, int y)
{
    if (!m_mappedData)
        return std::nullopt;

    auto decompressFn = [this](const std::string& data, uint8_t compression) {
        return this->decompress(data, compression);
    };

    try {
        auto [offset, length] = pmtiles::get_tile(
            decompressFn, m_mappedData,
            static_cast<uint8_t>(zoom),
            static_cast<uint32_t>(x),
            static_cast<uint32_t>(y));

        if (length == 0)
            return std::nullopt;

        if (offset + length > static_cast<uint64_t>(m_fileSize))
            return std::nullopt;

        QByteArray tileData(m_mappedData + offset, static_cast<int>(length));

        // Decompress tile data if needed
        if (m_header.tile_compression == pmtiles::COMPRESSION_GZIP) {
            auto decompressed = GzipUtils::decompress(tileData);
            if (!decompressed)
                return std::nullopt;
            return *decompressed;
        }

        return tileData;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::optional<int> PMTilesReader::tileSize(int zoom, int x, int y)
{
    if (!m_mappedData)
        return std::nullopt;

    auto decompressFn = [this](const std::string& data, uint8_t compression) {
        return this->decompress(data, compression);
    };

    try {
        auto [offset, length] = pmtiles::get_tile(
            decompressFn, m_mappedData,
            static_cast<uint8_t>(zoom),
            static_cast<uint32_t>(x),
            static_cast<uint32_t>(y));

        if (length == 0)
            return std::nullopt;

        return static_cast<int>(length);
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::string PMTilesReader::decompress(const std::string& data, uint8_t compression) const
{
    if (compression == pmtiles::COMPRESSION_NONE || compression == pmtiles::COMPRESSION_UNKNOWN)
        return data;

    if (compression == pmtiles::COMPRESSION_GZIP) {
        QByteArray input(data.data(), static_cast<int>(data.size()));
        auto result = GzipUtils::decompress(input);
        if (!result)
            throw std::runtime_error("gzip decompression failed");
        return std::string(result->data(), result->size());
    }

    throw std::runtime_error("unsupported compression type");
}
