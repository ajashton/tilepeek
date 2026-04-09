#include "map/RasterTileProvider.h"
#include "util/TileCoords.h"

#include <QBuffer>
#include <QImage>
#include <QImageReader>

RasterTileProvider::RasterTileProvider(std::unique_ptr<MBTilesReader> reader,
                                       const QString& formatHint,
                                       int minZoom, int maxZoom)
    : m_reader(std::move(reader))
    , m_formatHint(formatHint.toLower())
    , m_minZoom(minZoom)
    , m_maxZoom(maxZoom)
{
}

std::optional<QPixmap> RasterTileProvider::tileAt(int zoom, int x, int y)
{
    int tmsRow = TileCoords::xyzToTms(zoom, y);
    auto blob = m_reader->readTileData(zoom, x, tmsRow);
    if (!blob)
        return std::nullopt;

    QImage image;
    if (!image.loadFromData(*blob, m_formatHint.toLatin1().constData())) {
        // Format hint didn't work — try auto-detection
        if (!image.loadFromData(*blob))
            return std::nullopt;
    }
    return QPixmap::fromImage(std::move(image));
}

std::optional<int> RasterTileProvider::tileSizeAt(int zoom, int x, int y)
{
    int tmsRow = TileCoords::xyzToTms(zoom, y);
    auto blob = m_reader->readTileData(zoom, x, tmsRow);
    if (!blob)
        return std::nullopt;
    return static_cast<int>(blob->size());
}

int RasterTileProvider::detectNativeTileSize() const
{
    auto zoomRange = m_reader->queryZoomRange();
    if (!zoomRange)
        return 256;

    auto blob = m_reader->readTileData(zoomRange->minZoom, 0, 0);
    if (!blob)
        return 256;

    QImage image;
    if (image.loadFromData(*blob, m_formatHint.toLatin1().constData()))
        return image.width();
    if (image.loadFromData(*blob))
        return image.width();

    return 256;
}

FormatValidationResult RasterTileProvider::validateFormat()
{
    if (m_formatHint == "pbf") {
        return {FormatValidationResult::Status::Unsupported,
                "Vector tile format (pbf) is not yet supported"};
    }

    // Check if format is a recognized image format
    auto supported = QImageReader::supportedImageFormats();
    bool formatRecognized = false;
    for (const auto& fmt : supported) {
        if (QString::fromLatin1(fmt).toLower() == m_formatHint) {
            formatRecognized = true;
            break;
        }
    }

    if (!formatRecognized) {
        return {FormatValidationResult::Status::UnrecognizedFormat,
                "Unrecognized tile format: " + m_formatHint};
    }

    // Try to read a sample tile to validate format matches data
    auto zoomRange = m_reader->queryZoomRange();
    if (!zoomRange)
        return {FormatValidationResult::Status::Ok, {}};

    // Find the first available tile at minZoom
    // Try tile (0,0) at minZoom — if missing, the format check is inconclusive but OK
    auto blob = m_reader->readTileData(zoomRange->minZoom, 0, 0);
    if (!blob)
        return {FormatValidationResult::Status::Ok, {}};

    // Try loading with the declared format
    QImage image;
    if (image.loadFromData(*blob, m_formatHint.toLatin1().constData()))
        return {FormatValidationResult::Status::Ok, {}};

    // Declared format failed — try auto-detection
    QBuffer buffer(&*blob);
    buffer.open(QIODevice::ReadOnly);
    QImageReader autoReader(&buffer);
    if (autoReader.canRead()) {
        QString detected = QString::fromLatin1(autoReader.format());
        return {FormatValidationResult::Status::FormatMismatch,
                QString("Tile format metadata says '%1' but tiles appear to be '%2'")
                    .arg(m_formatHint, detected)};
    }

    return {FormatValidationResult::Status::FormatMismatch,
            "Tile data does not match declared format '" + m_formatHint
                + "' and could not be decoded"};
}
