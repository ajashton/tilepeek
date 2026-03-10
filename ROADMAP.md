# Roadmap

## v0.1

_Basic MBTiles metadata support_

- Load MBTiles SQLite file
- Display information from the `metadata` table/view in the UI
  - Display an Error toast if this table/view does not exist or does not contain `name` & `value` text columns
  - Display as a table split up into 4 sections: the "MUST contain" rows first, the "SHOULD contain" rows second, the "MAY contain" rows third, and any other rows last
- Use the `name` and `format` value in the window title
  - These formats are required by the spec; display an Error toast if they are not present
- Metadata fields recommended by the spec ("SHOULD contain") are `bounds`, `center`, `minzoom`, `maxzoom`; display a Warning toast if they are not present
  - Calculate these from the `tiles` table/view if they are missing
  - Check the `minzoom` & `maxzoom` values against the `tiles` table/view and display a warning if they are out of sync
  - Check that the `bounds` and `center` values can be parsed as comma-separated numbers; display an Error toast if not
  - Check that the parsed `bounds` and `center` coordinates are all within the valid ranges of ±180.0 for X and ±85.051129 for Y; display a Warning toast if they are not
- Check the `tiles` table/view for spec compatibility
  - Must contain these columns: zoom_level integer, tile_column integer, tile_row integer, tile_data blob
  - Display an Error toast if out of spec and refuse to continue loading/viewing the file
- Design consideration: parsing & validating metadata needs to be specific to the MBTiles spec, but the metadata class used for the UI should be format-agnostic so we can add support for PMTiles format in the future.

## v0.2

_Basic raster MBTiles display_

- Add a map view canvas to the app - this is a zoomable, pannable grid of images
  - Images are loaded and positioned according to the OpenStreetMap-compatible Web Mercator tiling scheme based on their Z/X/Y integer coordinates from the `zoom_level`, `tile_column`, and `tile_row`, values of the `tiles` table/view (with x=0, y=0 representing the northwest corner of each zoom level)
  - Only one zoom level is displayed at a time
  - The user can use their scroll wheel or trackpad to zoom in incrementally
  - Images for a zoom level are displayed scaled from 100% to 199% - zoom in any further and switch to the next zoom level
  - Images are only kept in memory for the current map view - if the user pans or zooms them out of sight they are unloaded
- Display the lowest-numbered zoom level by default, centered on the `center` metadata point
- Validate the `format` metadata value
  - If the value is `pbf`: display an Error toast that the format is not yet supported
  - If the value matches a supported QImageReader format, continue attempting to load & display the tiles
    - Display an Error toast if the `format` metadata value does not match the `tiles.tile_data` BLOB data.
    - If the BLOB data can be recognized as some other valid image format, display it anyway
  - For any other value: display an Error toast the the format is unrecognized
- Design consideration: we will need to support displaying vector tile data in the future; don't make tile loading & display too coupled to raster-specific concepts or methods.

## v0.3

_Additional tile info_

- Optionally display tile boundaries
- Optionally display tile Z/X/Y IDs
- Make missing tiles obvious (like a grey/red X through the tile)
- Optionally display a box for the `bounds` and dot for the `center`
- Calculate additional metadata
  - Number of tiles - total and broken down by zoom level
  - p50, p90, p99 tile data sizes - total and broken down by zoom level

## v0.4

_Basic Mapbox Vector Tile PBF parsing_

The following requirements all assume the user has opened a MBTiles file with the `format` metadata key set to `pbf`.

- Check that there is a `json` metadata key - required by the spec when format is `pbf`
  - Display an Error toast if it is either missing or does not parse as valid JSON
- Check the parsed JSON against the Vector Tileset Metadata section of the [MVT spec](https://github.com/mapbox/mbtiles-spec/blob/master/1.3/spec.md#vector-tileset-metadata)
  - a top-level `vector_layers` key is required
  - a top-level `tilestats` key is optional
  - display a Warning toast if there are any other top-level keys
- Do not display the `json` value with the rest of the tablular metadata; instead:
  - Add a "Layers" tab to the metadata sidebar for the `vector_layers` section
  - Add a "Stats` tab to the metadata sidebar for the `tilestats` section (if present)
  - Add a "Raw JSON" tab
- Attempt to parse `tile_data` blobs as Protocol Buffers using [the .proto file from the MVT spec](https://raw.githubusercontent.com/mapbox/vector-tile-spec/refs/heads/master/2.1/vector_tile.proto)

## v0.5

_Basic vector tile rendering_

- TBD: rendering strategy - we want performance, plus some flexibility to update rendering on the fly (e.g. to turn off/on vector layers, highlight selected features)
  - Use Qt's `QPainter` class to draw points, lines, and polygons?
    - If we go this route, should raster tiles be drawn using QPainter methods too? (eg `drawImage`, `drawPixmap`)
  - Convert tiles to SVG and display using `QImage`?
  - Something else?

## v0.6

_Inspect vector tile features_

- Clicking on a point, line, or polygon on the map canvas should reveal that feature's ID and associated properties

## v0.7

_PMTiles loading_

- Load metadata and raster or vector tiles from PMTiles according to [the v3 spec](https://raw.githubusercontent.com/protomaps/PMTiles/refs/heads/main/spec/v3/spec.md)

## v0.8

_Advanced tile stats_

- Exact stats TBD, but things like:
  - Tile data sizes broken down by vector layer
  - Data sizes broken down further by geometry, properties
  - p50, p90, p99 number of features per tile, or per layer
- Need to consider performance - full calculations would require parsing every vector tile and could be very slow for large tilesets
  - Calculate based on random sampling by default?

## v1.0

- Bug fixes & final polish
- Cross-platform testing
- Performance tuning
- User-friendly packaging & distribution (Flatpak, MacOS App Store, Windows App Store)

## TBD / Beyond v1.0

- Do we want to handle the optional `grids` & `grid_data` tables of MBTiles files?
- Should "fallback" overzooming be handled?
  - This is where, instead generating duplicate tiles for areas of empty land/water at all zoom levels, the map replaces missing tiles with stretched out copies from further up the pyramid
  - There is no mention of this in the MBTiles spec - how exactly do client libraries handle this?
  - Will want to clearly distinguish these from "proper" tiles to the viewer
  - Show the tile ID of the fallback tile?
- Load PMTiles from HTTP or S3 (using range requests)?
  - Some stats may be difficult to calculate without the whole file; may want to disable these
- Load individual tiles based on a TileJSON URL?
- Load individual tiles from a `{z}/{x}/{y}` URL template?
  - Certain metadata would be missing entirely
