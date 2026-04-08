#include "mvt/ProtobufDecoder.h"

#include <stdexcept>
#include <cstring>

namespace mvt {

ProtobufDecoder::ProtobufDecoder(const uint8_t* data, size_t size)
    : m_data(data)
    , m_end(data + size)
    , m_pos(data)
{
}

bool ProtobufDecoder::next()
{
    if (m_pos >= m_end)
        return false;

    uint64_t tag = readVarint();
    m_fieldNumber = static_cast<uint32_t>(tag >> 3);
    m_wireType = static_cast<uint32_t>(tag & 0x7);
    return true;
}

uint64_t ProtobufDecoder::readVarint()
{
    uint64_t result = 0;
    int shift = 0;
    while (m_pos < m_end) {
        uint8_t byte = *m_pos++;
        result |= static_cast<uint64_t>(byte & 0x7f) << shift;
        if ((byte & 0x80) == 0)
            return result;
        shift += 7;
        if (shift >= 64)
            throw std::runtime_error("varint too long");
    }
    throw std::runtime_error("unexpected end of data in varint");
}

uint32_t ProtobufDecoder::readFixed32()
{
    if (m_pos + 4 > m_end)
        throw std::runtime_error("unexpected end of data in fixed32");
    uint32_t result;
    std::memcpy(&result, m_pos, 4);
    m_pos += 4;
    return result;
}

uint64_t ProtobufDecoder::readFixed64()
{
    if (m_pos + 8 > m_end)
        throw std::runtime_error("unexpected end of data in fixed64");
    uint64_t result;
    std::memcpy(&result, m_pos, 8);
    m_pos += 8;
    return result;
}

std::pair<const uint8_t*, size_t> ProtobufDecoder::readBytes()
{
    uint64_t len = readVarint();
    if (m_pos + len > m_end)
        throw std::runtime_error("unexpected end of data in length-delimited field");
    auto* start = m_pos;
    m_pos += len;
    return {start, static_cast<size_t>(len)};
}

std::string_view ProtobufDecoder::readString()
{
    auto [data, size] = readBytes();
    return {reinterpret_cast<const char*>(data), size};
}

std::vector<uint32_t> ProtobufDecoder::readPackedUint32()
{
    auto [data, size] = readBytes();
    ProtobufDecoder sub(data, size);
    std::vector<uint32_t> result;
    while (!sub.atEnd())
        result.push_back(static_cast<uint32_t>(sub.readVarint()));
    return result;
}

void ProtobufDecoder::skip()
{
    switch (m_wireType) {
    case 0: // varint
        readVarint();
        break;
    case 1: // 64-bit
        if (m_pos + 8 > m_end)
            throw std::runtime_error("unexpected end of data");
        m_pos += 8;
        break;
    case 2: // length-delimited
        readBytes();
        break;
    case 5: // 32-bit
        if (m_pos + 4 > m_end)
            throw std::runtime_error("unexpected end of data");
        m_pos += 4;
        break;
    default:
        throw std::runtime_error("unknown wire type");
    }
}

int64_t ProtobufDecoder::zigzagDecode(uint64_t n)
{
    return static_cast<int64_t>((n >> 1) ^ (~(n & 1) + 1));
}

} // namespace mvt
