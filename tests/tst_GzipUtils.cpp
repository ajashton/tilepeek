#include "util/GzipUtils.h"

#include <QTest>
#include <zlib.h>

// Helper to gzip-compress data for testing
static QByteArray gzipCompress(const QByteArray& input)
{
    z_stream stream{};
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(input.data()));
    stream.avail_in = static_cast<uInt>(input.size());

    deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 16 + MAX_WBITS, 8, Z_DEFAULT_STRATEGY);

    QByteArray output;
    char buffer[32768];
    do {
        stream.next_out = reinterpret_cast<Bytef*>(buffer);
        stream.avail_out = sizeof(buffer);
        deflate(&stream, Z_FINISH);
        output.append(buffer, sizeof(buffer) - static_cast<int>(stream.avail_out));
    } while (stream.avail_out == 0);

    deflateEnd(&stream);
    return output;
}

class TestGzipUtils : public QObject {
    Q_OBJECT

private slots:
    void isGzipCompressed_gzipData()
    {
        QByteArray compressed = gzipCompress("hello world");
        QVERIFY(GzipUtils::isGzipCompressed(compressed));
    }

    void isGzipCompressed_plainData()
    {
        QVERIFY(!GzipUtils::isGzipCompressed("hello world"));
        QVERIFY(!GzipUtils::isGzipCompressed(QByteArray()));
        QVERIFY(!GzipUtils::isGzipCompressed(QByteArray(1, '\x1f')));
    }

    void decompressRoundTrip()
    {
        QByteArray original = "The quick brown fox jumps over the lazy dog";
        QByteArray compressed = gzipCompress(original);

        auto result = GzipUtils::decompress(compressed);
        QVERIFY(result.has_value());
        QCOMPARE(*result, original);
    }

    void decompressLargeData()
    {
        QByteArray original(100000, 'A');
        QByteArray compressed = gzipCompress(original);

        auto result = GzipUtils::decompress(compressed);
        QVERIFY(result.has_value());
        QCOMPARE(result->size(), original.size());
        QCOMPARE(*result, original);
    }

    void decompressCorruptData()
    {
        // Non-gzip data that starts with gzip magic but has garbage after
        QByteArray corrupt("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\x03\xDE\xAD", 12);
        auto result = GzipUtils::decompress(corrupt);
        QVERIFY(!result.has_value());
    }

    void decompressEmpty()
    {
        auto result = GzipUtils::decompress(QByteArray());
        QVERIFY(!result.has_value());
    }
};

QTEST_GUILESS_MAIN(TestGzipUtils)
#include "tst_GzipUtils.moc"
