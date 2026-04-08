#include "map/TileCache.h"

TileCache::TileCache(int capacity)
    : m_capacity(capacity)
{
}

void TileCache::insert(const TileKey& key, QPixmap pixmap)
{
    auto it = m_map.find(key);
    if (it != m_map.end()) {
        m_list.erase(it->second);
        m_map.erase(it);
    }

    m_list.emplace_front(key, std::move(pixmap));
    m_map[key] = m_list.begin();

    while (static_cast<int>(m_list.size()) > m_capacity) {
        auto& back = m_list.back();
        m_map.erase(back.first);
        m_list.pop_back();
    }
}

std::optional<QPixmap> TileCache::get(const TileKey& key)
{
    auto it = m_map.find(key);
    if (it == m_map.end())
        return std::nullopt;

    // Promote to front (MRU)
    m_list.splice(m_list.begin(), m_list, it->second);
    return it->second->second;
}

void TileCache::clear()
{
    m_list.clear();
    m_map.clear();
}

int TileCache::size() const
{
    return static_cast<int>(m_list.size());
}

void TileCache::evictOtherZooms(int keepZoom)
{
    for (auto it = m_list.begin(); it != m_list.end();) {
        if (it->first.zoom != keepZoom) {
            m_map.erase(it->first);
            it = m_list.erase(it);
        } else {
            ++it;
        }
    }
}
