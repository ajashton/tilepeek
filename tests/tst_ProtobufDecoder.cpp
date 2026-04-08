#include "mvt/ProtobufDecoder.h"

#include <QTest>

using namespace mvt;

class TestProtobufDecoder : public QObject {
    Q_OBJECT

private slots:
    void singleByteVarint()
    {
        // field 1, wire type 0, value 150
        // tag: (1 << 3) | 0 = 0x08
        // 150 encoded as varint: 0x96, 0x01
        uint8_t data[] = {0x08, 0x96, 0x01};
        ProtobufDecoder dec(data, sizeof(data));

        QVERIFY(dec.next());
        QCOMPARE(dec.fieldNumber(), 1u);
        QCOMPARE(dec.wireType(), 0u);
        QCOMPARE(dec.readVarint(), 150ull);
        QVERIFY(!dec.next());
    }

    void multiByteVarint()
    {
        // Value 300: 0xAC 0x02
        uint8_t data[] = {0x08, 0xAC, 0x02};
        ProtobufDecoder dec(data, sizeof(data));

        QVERIFY(dec.next());
        QCOMPARE(dec.readVarint(), 300ull);
    }

    void zigzagDecode()
    {
        QCOMPARE(ProtobufDecoder::zigzagDecode(0), 0ll);
        QCOMPARE(ProtobufDecoder::zigzagDecode(1), -1ll);
        QCOMPARE(ProtobufDecoder::zigzagDecode(2), 1ll);
        QCOMPARE(ProtobufDecoder::zigzagDecode(3), -2ll);
        QCOMPARE(ProtobufDecoder::zigzagDecode(4), 2ll);
        QCOMPARE(ProtobufDecoder::zigzagDecode(4294967294ull), 2147483647ll);
        QCOMPARE(ProtobufDecoder::zigzagDecode(4294967295ull), -2147483648ll);
    }

    void fixed32()
    {
        // field 2, wire type 5 (32-bit), value 0x01020304
        uint8_t data[] = {0x15, 0x04, 0x03, 0x02, 0x01};
        ProtobufDecoder dec(data, sizeof(data));

        QVERIFY(dec.next());
        QCOMPARE(dec.fieldNumber(), 2u);
        QCOMPARE(dec.wireType(), 5u);
        QCOMPARE(dec.readFixed32(), 0x01020304u);
    }

    void fixed64()
    {
        // field 1, wire type 1 (64-bit)
        uint8_t data[] = {0x09, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
        ProtobufDecoder dec(data, sizeof(data));

        QVERIFY(dec.next());
        QCOMPARE(dec.wireType(), 1u);
        QCOMPARE(dec.readFixed64(), 0x0807060504030201ull);
    }

    void lengthDelimited()
    {
        // field 1, wire type 2, length 3, data "abc"
        uint8_t data[] = {0x0A, 0x03, 'a', 'b', 'c'};
        ProtobufDecoder dec(data, sizeof(data));

        QVERIFY(dec.next());
        QCOMPARE(dec.wireType(), 2u);
        auto str = dec.readString();
        QCOMPARE(str, std::string_view("abc"));
    }

    void packedUint32()
    {
        // field 4, wire type 2 (packed), values: 3, 270, 86942
        // 270 = 0x8E 0x02, 86942 = 0x9E 0xA7 0x05
        uint8_t data[] = {0x22, 0x06, 0x03, 0x8E, 0x02, 0x9E, 0xA7, 0x05};
        ProtobufDecoder dec(data, sizeof(data));

        QVERIFY(dec.next());
        auto packed = dec.readPackedUint32();
        QCOMPARE(packed.size(), 3u);
        QCOMPARE(packed[0], 3u);
        QCOMPARE(packed[1], 270u);
        QCOMPARE(packed[2], 86942u);
    }

    void skipFields()
    {
        // field 1 varint (value 5), field 2 string "hi", field 3 varint (value 99)
        uint8_t data[] = {0x08, 0x05, 0x12, 0x02, 'h', 'i', 0x18, 0x63};
        ProtobufDecoder dec(data, sizeof(data));

        QVERIFY(dec.next()); // field 1
        dec.skip();
        QVERIFY(dec.next()); // field 2
        dec.skip();
        QVERIFY(dec.next()); // field 3
        QCOMPARE(dec.fieldNumber(), 3u);
        QCOMPARE(dec.readVarint(), 99ull);
    }

    void multipleFields()
    {
        // field 1 varint 42, field 15 varint 2
        // field 15 tag: (15 << 3) | 0 = 120 = 0x78
        uint8_t data[] = {0x08, 0x2A, 0x78, 0x02};
        ProtobufDecoder dec(data, sizeof(data));

        QVERIFY(dec.next());
        QCOMPARE(dec.fieldNumber(), 1u);
        QCOMPARE(dec.readVarint(), 42ull);

        QVERIFY(dec.next());
        QCOMPARE(dec.fieldNumber(), 15u);
        QCOMPARE(dec.readVarint(), 2ull);

        QVERIFY(!dec.next());
    }

    void emptyBuffer()
    {
        ProtobufDecoder dec(nullptr, 0);
        QVERIFY(!dec.next());
        QVERIFY(dec.atEnd());
    }
};

QTEST_GUILESS_MAIN(TestProtobufDecoder)
#include "tst_ProtobufDecoder.moc"
