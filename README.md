# tilepeek - a map tile viewer

TilePeek a tool for quickly and easily inspecting map tilesets from local files.

## Prior art / alternatives

- [QGIS](https://qgis.org/) - desktop, supports local PMTiles/MBTiles and remote `{z}/{x}/{y}` URL templates
  - Local PMTiles viewing seems very slow?
  - Displaying tile IDs requires a plugin
- [pmtiles.io](https://pmtiles.io/) - web-based, supports local and remote files, PMTiles-only
- [go-pmtiles](https://github.com/protomaps/go-pmtiles) - CLI, inspect metadata only (no map view), supports local and remote files, PMTiles only
- [Martin mbtiles](https://maplibre.org/martin/tools/) - CLI, inspect metadata only (no map view), MBTiles-only
