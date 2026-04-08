#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace mvt {

enum class GeomType : uint8_t {
    Unknown = 0,
    Point = 1,
    LineString = 2,
    Polygon = 3,
};

using Value = std::variant<std::string, float, double, int64_t, uint64_t, bool>;

struct Feature {
    std::optional<uint64_t> id;
    GeomType type = GeomType::Unknown;
    std::vector<uint32_t> tags;
    std::vector<uint32_t> geometry;
};

struct Layer {
    std::string name;
    uint32_t version = 2;
    uint32_t extent = 4096;
    std::vector<Feature> features;
    std::vector<std::string> keys;
    std::vector<Value> values;
};

struct Tile {
    std::vector<Layer> layers;
};

} // namespace mvt
