#include "mvt/MvtDecoder.h"

#include <QByteArray>
#include <QTest>

using namespace mvt;

// Helper to build protobuf bytes
class PbWriter {
public:
    void writeVarint(uint64_t val)
    {
        while (val > 0x7f) {
            m_buf.append(static_cast<char>((val & 0x7f) | 0x80));
            val >>= 7;
        }
        m_buf.append(static_cast<char>(val));
    }

    void writeTag(uint32_t fieldNumber, uint32_t wireType)
    {
        writeVarint((fieldNumber << 3) | wireType);
    }

    void writeString(uint32_t fieldNumber, const std::string& str)
    {
        writeTag(fieldNumber, 2);
        writeVarint(str.size());
        m_buf.append(str.data(), str.size());
    }

    void writeSubmessage(uint32_t fieldNumber, const QByteArray& sub)
    {
        writeTag(fieldNumber, 2);
        writeVarint(sub.size());
        m_buf.append(sub);
    }

    void writeVarintField(uint32_t fieldNumber, uint64_t val)
    {
        writeTag(fieldNumber, 0);
        writeVarint(val);
    }

    void writePackedUint32(uint32_t fieldNumber, const std::vector<uint32_t>& vals)
    {
        PbWriter inner;
        for (uint32_t v : vals)
            inner.writeVarint(v);
        writeSubmessage(fieldNumber, inner.data());
    }

    QByteArray data() const { return m_buf; }

private:
    QByteArray m_buf;
};

class TestMvtDecoder : public QObject {
    Q_OBJECT

private slots:
    void decodeSinglePointFeature()
    {
        // Build a Value submessage (string "hello")
        PbWriter valWriter;
        valWriter.writeString(1, "hello");

        // Build a Feature submessage
        PbWriter featWriter;
        featWriter.writeVarintField(1, 42);            // id = 42
        featWriter.writeVarintField(3, 1);             // type = POINT
        featWriter.writePackedUint32(2, {0, 0});       // tags: key[0], val[0]
        // geometry: MoveTo(1) + dx=25, dy=17
        // MoveTo command: (1 & 0x7) | (1 << 3) = 9
        // zigzag(25) = 50, zigzag(17) = 34
        featWriter.writePackedUint32(4, {9, 50, 34});

        // Build a Layer submessage
        PbWriter layerWriter;
        layerWriter.writeVarintField(15, 2);           // version = 2
        layerWriter.writeString(1, "testlayer");       // name
        layerWriter.writeSubmessage(2, featWriter.data()); // feature
        layerWriter.writeString(3, "name");            // keys[0]
        layerWriter.writeSubmessage(4, valWriter.data()); // values[0]
        layerWriter.writeVarintField(5, 4096);         // extent

        // Build a Tile
        PbWriter tileWriter;
        tileWriter.writeSubmessage(3, layerWriter.data());

        auto result = decodeTile(tileWriter.data());
        QVERIFY(result.tile.has_value());
        QVERIFY(result.error.isEmpty());

        auto& tile = *result.tile;
        QCOMPARE(tile.layers.size(), 1u);

        auto& layer = tile.layers[0];
        QCOMPARE(layer.name, std::string("testlayer"));
        QCOMPARE(layer.version, 2u);
        QCOMPARE(layer.extent, 4096u);
        QCOMPARE(layer.keys.size(), 1u);
        QCOMPARE(layer.keys[0], std::string("name"));
        QCOMPARE(layer.values.size(), 1u);
        QVERIFY(std::holds_alternative<std::string>(layer.values[0]));
        QCOMPARE(std::get<std::string>(layer.values[0]), std::string("hello"));

        QCOMPARE(layer.features.size(), 1u);
        auto& feat = layer.features[0];
        QVERIFY(feat.id.has_value());
        QCOMPARE(*feat.id, 42ull);
        QCOMPARE(feat.type, GeomType::Point);
        QCOMPARE(feat.tags.size(), 2u);
        QCOMPARE(feat.geometry.size(), 3u);
        QCOMPARE(feat.geometry[0], 9u); // MoveTo(1)
    }

    void decodeMultipleLayers()
    {
        PbWriter layer1;
        layer1.writeVarintField(15, 2);
        layer1.writeString(1, "roads");

        PbWriter layer2;
        layer2.writeVarintField(15, 2);
        layer2.writeString(1, "buildings");

        PbWriter tileWriter;
        tileWriter.writeSubmessage(3, layer1.data());
        tileWriter.writeSubmessage(3, layer2.data());

        auto result = decodeTile(tileWriter.data());
        QVERIFY(result.tile.has_value());
        QCOMPARE(result.tile->layers.size(), 2u);
        QCOMPARE(result.tile->layers[0].name, std::string("roads"));
        QCOMPARE(result.tile->layers[1].name, std::string("buildings"));
    }

    void decodeEmptyTile()
    {
        auto result = decodeTile(QByteArray());
        QVERIFY(result.tile.has_value());
        QCOMPARE(result.tile->layers.size(), 0u);
    }

    void decodeCorruptData()
    {
        // Truncated varint
        QByteArray corrupt("\x1a\x80", 2);
        auto result = decodeTile(corrupt);
        QVERIFY(!result.tile.has_value());
        QVERIFY(!result.error.isEmpty());
    }

    void decodeIntegerValueTypes()
    {
        // Build values of different types
        PbWriter intVal;
        intVal.writeVarintField(4, 42); // int_value

        PbWriter uintVal;
        uintVal.writeVarintField(5, 100); // uint_value

        PbWriter boolVal;
        boolVal.writeVarintField(7, 1); // bool_value = true

        PbWriter layerWriter;
        layerWriter.writeVarintField(15, 2);
        layerWriter.writeString(1, "test");
        layerWriter.writeSubmessage(4, intVal.data());
        layerWriter.writeSubmessage(4, uintVal.data());
        layerWriter.writeSubmessage(4, boolVal.data());

        PbWriter tileWriter;
        tileWriter.writeSubmessage(3, layerWriter.data());

        auto result = decodeTile(tileWriter.data());
        QVERIFY(result.tile.has_value());

        auto& vals = result.tile->layers[0].values;
        QCOMPARE(vals.size(), 3u);
        QVERIFY(std::holds_alternative<int64_t>(vals[0]));
        QCOMPARE(std::get<int64_t>(vals[0]), 42ll);
        QVERIFY(std::holds_alternative<uint64_t>(vals[1]));
        QCOMPARE(std::get<uint64_t>(vals[1]), 100ull);
        QVERIFY(std::holds_alternative<bool>(vals[2]));
        QCOMPARE(std::get<bool>(vals[2]), true);
    }
};

QTEST_GUILESS_MAIN(TestMvtDecoder)
#include "tst_MvtDecoder.moc"
