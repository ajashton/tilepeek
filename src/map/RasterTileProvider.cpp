#include "map/RasterTileProvider.h"

#include <QBuffer>
#include <QImage>
#include <QImageReader>
#include <QMutexLocker>

RasterTileProvider::RasterTileProvider(std::unique_ptr<TileSource> source,
                                       const QString& formatHint,
                                       int minZoom, int maxZoom)
    : m_source(std::move(source))
    , m_formatHint(formatHint.toLower())
    , m_minZoom(minZoom)
    , m_maxZoom(maxZoom)
{
}

std::optional<QImage> RasterTileProvider::tileAt(int zoom, int x, int y)
{
    std::optional<QByteArray> blob;
    {
        QMutexLocker lock(&m_sourceMutex);
        blob = m_source->readTile(zoom, x, y);
    }
    if (!blob)
        return std::nullopt;

    QImage image;
    if (!image.loadFromData(*blob, m_formatHint.toLatin1().constData())) {
        if (!image.loadFromData(*blob))
            return std::nullopt;
    }
    return image;
}

std::optional<int> RasterTileProvider::tileSizeAt(int zoom, int x, int y)
{
    QMutexLocker lock(&m_sourceMutex);
    return m_source->tileSize(zoom, x, y);
}

int RasterTileProvider::detectNativeTileSize() const
{
    // Try a few tile coordinates at minZoom to find a sample tile
    auto blob = m_source->readTile(m_minZoom, 0, 0);
    if (!blob) {
        // Try center tile
        int maxTile = (1 << m_minZoom) / 2;
        blob = m_source->readTile(m_minZoom, maxTile, maxTile);
    }
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
    auto blob = m_source->readTile(m_minZoom, 0, 0);
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
