# Authored cell files

Each `.cell` file represents one independently stored space. It has UTF-8 header lines followed by an ASCII terrain grid; every grid row must have the same width. The standalone cell parser allows at most 512×512 terrain tiles, while Atlas Workshop and custom-world manifest import deliberately limit individual cells/rooms to 256×256. A tile is a spacious drawing patch. Entity coordinates remain continuous inside it.

| Header | Meaning |
| --- | --- |
| `id` | Stable cell identity |
| `name` | Display name |
| `description` | Scene prose |
| `world` | Relative X Y Z coordinates for neighborhood map |
| `outdoors` | `true` enables weather effects on perception/movement |
| `weather` | `clear`, `rain`, `fog`, or `snow` |
| `wind` | Optional airflow direction in radians, strength 0–1, and variation flag 0/1; exported interiors are calm |
| `lighting` | Optional `artificial daylightAccess tone`; both levels finite 0–1, tone `warm`, `neutral`, or `cool`. Legacy default `1 1 warm`; outdoors uses sky illumination instead |
| `height` | Repeatable `x y value` sparse elevation override; −16..16 in quarter-tile steps |
| `grid:` | All following lines form terrain rows; blank rows are invalid |

| Glyph | Interpretation |
| --- | --- |
| `#` | Solid, opaque wall or dense cover |
| `.` | Open floor or path |
| `,` / `"` | Grass with modest movement cost |
| `T` | Solid, low table; does not block sight |
| `=` | Solid, low counter; does not block sight |
| `~` | Traversable shallow water; high movement cost |
| `:` | Quarter-height approach to a rise |
| `^` | Half-height step/rise; also used under stair fixtures |
| `+` | Door location; the separate door fixture determines open/closed state |

`World::loadCellFile(path)` validates rectangularity and prevents stranding current actors inside solid tiles. Load these before restoring world state. The three built-in demo cells are identical fallbacks for packaged builds that do not ship external content. Their demonstration fixture/portal definitions and six NPCs remain authored in `World::createDemo`; tests compare file data to the fallback cells to catch drift. Atlas exports now supply their own versioned fixture/portal records in a world manifest, but do not author NPCs or items.

No client can invent a new cell by sending this format: only trusted server startup code calls the loader. Reloading map geometry while players are connected is not a supported authoring workflow.

For an interior, `lighting: 1 1 warm` gives a clear tavern with warm glow after sunset and no glow in full daylight. `lighting: 0 1 warm` is window-lit only; `lighting: 0 0 warm` stays dark at noon. Lighting is whole-cell ambient, not a lamp-position system. Darkness reduces actual sight but does not remove indoor shelter for hearing, scent or movement.

## Atlas Workshop exports

Run `python3 tools/map_editor.py serve` from the repository and open its printed
loopback URL. Paint the shared world canvas, cut the default 32×24 or a custom
grid, merge/split rectangles, then drill down for fine terrain/elevation work and
door-linking to detached rooms. The current editor opens its authoring JSON;
it does **not** import legacy `.cell` files from this directory.

An exported ZIP contains separate `cells/<id>.cell` files, `world.ratw`, the
editable `atlas.json`, and launch notes. The world canvas and individual
cells/rooms are rectangular and at most 256×256 tiles each, with at most 256 total
cells/rooms. The exporter generates reciprocal compatible edge passages and
explicit door/stair links; cuts do not insert new terrain walls. None of this
changes the player's current-cell-only local map or established transition rules.

Extract an export into a new directory and opt into it with
`-RatwWorld=/absolute/path/to/world.ratw`. `World::loadWorldFile` validates the
manifest, contained cell paths, fixtures, and spawn into a candidate before
replacing demonstration content. A custom import begins without the demo NPCs.

Custom-save identity uses a hash of the manifest **path** rather than a content
hash. A geometry/topology revision therefore needs a new export directory or a
fresh explicit `-RatwSave` path. There is no live reload or save migration, and
exporting does not overwrite the built-in demonstration files or a previous
export directory. See `Docs/Design/12-map-editor.md` and `Docs/EDITOR_CONTRACT.md`.
