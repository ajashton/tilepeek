#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

namespace mvt {

class ProtobufDecoder {
public:
    ProtobufDecoder(const uint8_t* data, size_t size);

    bool next();
    uint32_t fieldNumber() const { return m_fieldNumber; }
    uint32_t wireType() const { return m_wireType; }

    uint64_t readVarint();
    uint32_t readFixed32();
    uint64_t readFixed64();
    std::pair<const uint8_t*, size_t> readBytes();
    std::string_view readString();
    std::vector<uint32_t> readPackedUint32();
    void skip();

    bool atEnd() const { return m_pos >= m_end; }

    static int64_t zigzagDecode(uint64_t n);

private:
    const uint8_t* m_data;
    const uint8_t* m_end;
    const uint8_t* m_pos;
    uint32_t m_fieldNumber = 0;
    uint32_t m_wireType = 0;
};

} // namespace mvt
