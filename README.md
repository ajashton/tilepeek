# TilePeek

TilePeek is a desktop application for previewing & inspecting map tilesets from local files. For when you just need a quick peek and don't want to fire up a full session of QGIS, Maputnik, etc.

> [!WARNING]
> This software is in early stages of development. There is no packaged or supported release yet.

## Feature overview

- Open local tilesets from MBTiles v1 & PMTiles v3 container formats
- View raster tiles from PNG, JPEG, WebP, and more formats
  - _full list of supported formats depends on your Qt installation_
- View & inspect vector tile data in [Mapbox Vector Tile (MVT)](https://github.com/mapbox/vector-tile-spec) v2 format
  - _future [MapLibre Tile (MLT)](https://maplibre.org/maplibre-tile-spec/) support is likely but not prioritized_
- View tileset metadata & tile statistics (sizes, counts)
- Optionally visualize tile boundaries, tile IDs, tileset bounds & center
- Supports uncompressed and gzip-compressed tile data
  - _zstd & brotli support coming soon_

## Prior art / alternatives

- [QGIS](https://qgis.org/) - desktop, supports local PMTiles/MBTiles and remote `{z}/{x}/{y}` URL templates
  - Local PMTiles viewing seems very slow?
  - Displaying tile IDs requires a plugin
- [pmtiles.io](https://pmtiles.io/) - web-based, supports local and remote files, PMTiles-only
- [go-pmtiles](https://github.com/protomaps/go-pmtiles) - CLI, inspect metadata only (no map view), supports local and remote files, PMTiles only
- [Martin mbtiles](https://maplibre.org/martin/tools/) - CLI, inspect metadata only (no map view), MBTiles-only
