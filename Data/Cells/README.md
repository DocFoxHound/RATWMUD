# Authored cell files

Each `.cell` file represents one independently stored space. It has UTF-8 header lines followed by an ASCII terrain grid; every grid row must have the same width. The loader allows at most 512×512 terrain tiles. A tile is a spacious drawing patch. Entity coordinates remain continuous inside it.

| Header | Meaning |
| --- | --- |
| `id` | Stable cell identity |
| `name` | Display name |
| `description` | Scene prose |
| `world` | Relative X Y Z coordinates for neighborhood map |
| `outdoors` | `true` enables weather effects on perception/movement |
| `weather` | `clear`, `rain`, `fog`, or `snow` |
| `grid:` | All following nonempty lines form terrain rows |

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

`World::loadCellFile(path)` validates rectangularity and prevents stranding current actors inside solid tiles. Load these before restoring world state. The three built-in demo cells are identical fallbacks for packaged builds that do not ship external content. Their fixture/portal definitions and six NPCs are currently authored in `World::createDemo`; a production content editor must move those definitions into versioned external records too. The tests compare file data to the fallback cells to catch drift.

No client can invent a new cell by sending this format: only trusted server startup code calls the loader. Reloading map geometry while players are connected is not a supported authoring workflow.
