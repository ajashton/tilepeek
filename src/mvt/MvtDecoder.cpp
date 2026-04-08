#include "mvt/MvtDecoder.h"
#include "mvt/ProtobufDecoder.h"

#include <QByteArray>
#include <cstring>
#include <stdexcept>

namespace mvt {

static Value decodeValue(const uint8_t* data, size_t size)
{
    ProtobufDecoder dec(data, size);
    Value result;
    bool found = false;

    while (dec.next()) {
        switch (dec.fieldNumber()) {
        case 1: { // string_value
            auto sv = dec.readString();
            result = std::string(sv);
            found = true;
            break;
        }
        case 2: { // float_value
            uint32_t bits = dec.readFixed32();
            float f;
            std::memcpy(&f, &bits, 4);
            result = f;
            found = true;
            break;
        }
        case 3: { // double_value
            uint64_t bits = dec.readFixed64();
            double d;
            std::memcpy(&d, &bits, 8);
            result = d;
            found = true;
            break;
        }
        case 4: // int_value
            result = static_cast<int64_t>(dec.readVarint());
            found = true;
            break;
        case 5: // uint_value
            result = dec.readVarint();
            found = true;
            break;
        case 6: { // sint_value
            uint64_t raw = dec.readVarint();
            result = ProtobufDecoder::zigzagDecode(raw);
            found = true;
            break;
        }
        case 7: // bool_value
            result = dec.readVarint() != 0;
            found = true;
            break;
        default:
            dec.skip();
            break;
        }
    }

    if (!found)
        result = std::string();
    return result;
}

static Feature decodeFeature(const uint8_t* data, size_t size)
{
    ProtobufDecoder dec(data, size);
    Feature feature;

    while (dec.next()) {
        switch (dec.fieldNumber()) {
        case 1: // id
            feature.id = dec.readVarint();
            break;
        case 2: // tags (packed)
            feature.tags = dec.readPackedUint32();
            break;
        case 3: // type
            feature.type = static_cast<GeomType>(dec.readVarint());
            break;
        case 4: // geometry (packed)
            feature.geometry = dec.readPackedUint32();
            break;
        default:
            dec.skip();
            break;
        }
    }

    return feature;
}

static Layer decodeLayer(const uint8_t* data, size_t size)
{
    ProtobufDecoder dec(data, size);
    Layer layer;

    while (dec.next()) {
        switch (dec.fieldNumber()) {
        case 1: { // name
            auto sv = dec.readString();
            layer.name = std::string(sv);
            break;
        }
        case 2: { // feature (submessage)
            auto [fdata, fsize] = dec.readBytes();
            layer.features.push_back(decodeFeature(fdata, fsize));
            break;
        }
        case 3: { // keys
            auto sv = dec.readString();
            layer.keys.emplace_back(sv);
            break;
        }
        case 4: { // values (submessage)
            auto [vdata, vsize] = dec.readBytes();
            layer.values.push_back(decodeValue(vdata, vsize));
            break;
        }
        case 5: // extent
            layer.extent = static_cast<uint32_t>(dec.readVarint());
            break;
        case 15: // version
            layer.version = static_cast<uint32_t>(dec.readVarint());
            break;
        default:
            dec.skip();
            break;
        }
    }

    return layer;
}

DecodeResult decodeTile(const QByteArray& data)
{
    try {
        auto* bytes = reinterpret_cast<const uint8_t*>(data.constData());
        ProtobufDecoder dec(bytes, static_cast<size_t>(data.size()));
        Tile tile;

        while (dec.next()) {
            switch (dec.fieldNumber()) {
            case 3: { // layer (submessage)
                auto [ldata, lsize] = dec.readBytes();
                tile.layers.push_back(decodeLayer(ldata, lsize));
                break;
            }
            default:
                dec.skip();
                break;
            }
        }

        return {std::move(tile), {}};
    } catch (const std::exception& e) {
        return {std::nullopt, QString::fromStdString(e.what())};
    }
}

} // namespace mvt
