// SPDX-FileCopyrightText: 2026 AJ Ashton <aj@ajashton.ca>
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QPixmap>
#include <list>
#include <optional>
#include <unordered_map>

struct TileKey {
    int zoom, x, y;
    bool operator==(const TileKey&) const = default;
};

struct TileKeyHash {
    std::size_t operator()(const TileKey& k) const
    {
        std::size_t h = std::hash<int>{}(k.zoom);
        h ^= std::hash<int>{}(k.x) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<int>{}(k.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

class TileCache {
public:
    struct Entry {
        TileKey key;
        QPixmap pixmap;
        int generation;
    };

    explicit TileCache(int capacity = 384);

    void insert(const TileKey& key, QPixmap pixmap, int generation = 0);
    std::optional<QPixmap> get(const TileKey& key);
    std::optional<std::pair<QPixmap, int>> getWithGeneration(const TileKey& key);
    void clear();
    int size() const;
    void evictOtherZooms(int keepZoom);

private:
    int m_capacity;
    std::list<Entry> m_list;
    std::unordered_map<TileKey, decltype(m_list)::iterator, TileKeyHash> m_map;
};
