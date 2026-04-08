# Roadmap

## v0.1

_Basic MBTiles metadata support_

- Ability to load & read a MBTiles file from disk using any of the following methods:
  - File->Open menu
  - Drag & drop file on to main app window
  - First positional command line argument
- The app can only open one MBTiles file at a time
- MBTiles is a standardized SQLite schema - see `reference/MBTiles-v1.3.md`
- Display information from the `metadata` table/view in the UI
  - Display an Error toast if this table/view does not exist or does not contain `name` & `value` text columns
  - Display as a table split up into 4 sections: the "MUST contain" rows first, the "SHOULD contain" rows second, the "MAY contain" rows third, and any other rows last
      - These concepts should be abstracted to fit additional formats/standards in the future. We could call them "Required", "Recommended", "Standard", and "Non-standard". We don't necessarily have to label these in the UI; they could just be separated by spacing or separator lines
  - UI: metadata is shown in a sidebar panel, with the bulk of the app window being an empty placeholder for a map display that will be implemented later
- Metadata fields recommended by the spec ("SHOULD contain") are `bounds`, `center`, `minzoom`, `maxzoom`; display a Warning toast if they are not present
  - Calculate these from the `tiles` table/view if they are missing
  - Check the `minzoom` & `maxzoom` values against the `tiles` table/view and display a warning if they are out of sync
  - Check that the `bounds` and `center` values can be parsed as comma-separated numbers; display an Error toast if not
  - Check that the parsed `bounds` and `center` coordinates are all within the valid ranges of ±180.0 for X and ±85.051129 for Y; display a Warning toast if they are not
- Check the `tiles` table/view for spec compatibility
  - Must contain these columns: zoom_level integer, tile_column integer, tile_row integer, tile_data blob
  - Display an Error toast if out of spec and refuse to continue loading/viewing the file
- Y-axis convention: MBTiles uses TMS convention where y=0 is the **southwest** corner. Internally, the application should use north-to-south Y ordering (y=0 = northwest corner) to match Qt's `left,top` coordinate system. MBTiles `tile_row` values must be flipped on read: `y = (2^zoom - 1) - tile_row`. Tile coordinates displayed to the user should also default to the common north-to-south Z/X/Y format used in web map URLs.
- Design consideration: parsing & validating metadata needs to be specific to the MBTiles spec, but the metadata class used for the UI should be format-agnostic so we can add support for PMTiles format in the future.

## v0.2

_Basic raster MBTiles display_

- Add a map view canvas to the app - this is a zoomable, pannable grid of images
  - Images are loaded and positioned according to the Web Mercator tiling scheme based on their Z/X/Y integer coordinates (with x=0, y=0 at the northwest corner of each zoom level — see v0.1 Y-axis convention note for how MBTiles `tile_row` maps to this)
  - Only one zoom level is displayed at a time
  - The user can use their scroll wheel or trackpad to zoom in incrementally
  - Images for a zoom level are displayed scaled from 100% to 199% — zoom in further and the map transitions to the next higher zoom level. Zoom out past 100% and the map transitions to the next lower zoom level (displayed at ~200% scale of that level). The map does not zoom beyond the available `minzoom`/`maxzoom` range.
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
  - These calculations should run asynchronously to keep the UI responsive on large tilesets

## v0.4

_Basic Mapbox Vector Tile PBF parsing_

The following requirements all assume the user has opened a MBTiles file with the `format` metadata key set to `pbf`.

- Decompress gzip-compressed `tile_data` BLOBs before parsing (gzip compression is required by the MBTiles spec for `pbf` format)
- Check that there is a `json` metadata key - required by the spec when format is `pbf`
  - Display an Error toast if it is either missing or does not parse as valid JSON
- Check the parsed JSON against the Vector Tileset Metadata section of the MVT spec
  - a top-level `vector_layers` key is required
  - a top-level `tilestats` key is optional
  - display a Warning toast if there are any other top-level keys
- Do not display the `json` value with the rest of the tabular metadata; instead:
  - Add a "Layers" tab to the metadata sidebar for the `vector_layers` section
  - Add a "Stats" tab to the metadata sidebar for the `tilestats` section (if present)
  - Add a "Raw JSON" tab
- Parse `tile_data` BLOBs as Protocol Buffers according to the MVT spec
  - Use a lightweight, custom protobuf decoder purpose-built for reading MVT data — we only need to decode, not encode, and only need to support the protobuf features used by the MVT schema. See [protozero](https://github.com/mapbox/protozero) for reference/inspiration.
  - Parsed data should be structured to serve the rendering needs of v0.5 and the feature inspection needs of v0.6 (layers, features with geometry + properties + IDs)
  - Display basic per-tile summary stats on the map canvas: feature count per layer

## v0.5

_Basic vector tile rendering_

- Render vector tile geometry using Qt's `QPainter`
  - This is the best fit for our goals: zero additional dependencies, hardware-accelerated 2D drawing, full control for interactive features, and MVT geometry commands (MoveTo/LineTo/ClosePath) map almost directly to `QPainterPath` operations
  - Unify the rendering path: raster tiles should also be drawn via `QPainter` (`drawPixmap`) so both tile types share one canvas implementation
- Rendering pipeline:
  1. Parsed MVT data (from v0.4) provides layer/feature/geometry structs
  2. Transform tile-local coordinates (0–extent, typically 0–4096) to screen pixel coordinates
  3. Build `QPainterPath` objects from the MVT command stream
  4. Draw with default styles per layer
- Default styling:
  - Assign a distinct color per vector layer from a built-in palette
  - Polygons: semi-transparent fill with solid stroke
  - Lines: solid colored stroke
  - Points: small filled circles
- Enable anti-aliasing by default (`QPainter::Antialiasing` render hint)
- Cache rendered vector tiles to `QPixmap` — invalidate on pan/zoom to avoid re-drawing the same geometry every paint event
- Support toggling individual vector layers on/off via the Layers sidebar

## v0.6

_Inspect vector tile features_

- Clicking on a point, line, or polygon on the map canvas should reveal that feature's ID and associated properties
- Exact design TBD
  - Sidebar/panel? Tooltip/popup over map?
  - Need to handle overlapping features

## v0.7

_PMTiles support_

- Load metadata and raster or vector tiles from PMTiles according to the spec - see `reference/PMTiles-v3.5.md`
- Consider vendoring the [official C++ library](https://github.com/protomaps/PMTiles/tree/main/cpp)

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
- Should we be lenient about compression in MBTiles? The 1.3 spec requires gzip compression, but some tools may produce uncompressed tiles or use other compression algorithms.
  - Less of an issue for PMTiles - multiple compression options are standardized in the spec
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
- Open more than one tileset at a time?
- Built-in background reference layers (eg Natural Earth coastlines, Blue Marble / Landsat satellite imagery)
- Zooming UI/UX
  - Prettier raster upscaling (eg bilinear)
  - Display the current zoom level
  - Provide a way to snap to the nearest integer zoom level
  - Double-click to zoom in by 1 zoom level (or: zoom in to the next integer zoom level?)
