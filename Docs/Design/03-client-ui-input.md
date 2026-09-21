# Client interface and input

Status: implemented native Unreal Slate vertical slice. See `Source/RATWMUD/UI/SRatwGame.*`.

## Intent

The window is a shared stage for text roleplay. The narrative occupies the left pane and the current cell occupies the larger right pane. Muted charcoal, warm amber, sage, and cool blue distinguish structure, self, residents, and other players. Equipment and character illustrations appear only in their focused panels.

The client accepts observer-filtered JSON snapshots and roleplay events from the player controller. All actions produce JSON intentions. It neither simulates movement nor reveals terrain or identity hidden by the server.

## Rendering

- A 1600×1000 reference canvas scales uniformly to the viewport and centers within other aspect ratios.
- Settings offers four pane-balance presets: compact, balanced, wide narrative and text-first. Story text reflows and the composer resizes; the map keeps square tiles instead of stretching its graphics. The pane-balance automation verifies bounds and draft retention without server commands.
- Terrain is independently drawn glyph tiles. Blank unknown tiles remain blank; remembered tiles use reduced opacity. Entities use continuous coordinates and interpolation between snapshots.
- Every wolf is an upright `W`; a separate literal `>` glyph rotates and orbits the center. The pointer has no collision. Identity colors are distinct from speaking colors.
- The local pane contains only the current cell. Larger cells preserve glyph scale and support wheel scrolling; Shift-wheel scrolls horizontally.
- The world pane includes only the server-supplied current cell and adjacent cells. Unknown cells are omitted, glimpsed cells have coarse outlines, and visited cells can show the supplied remembered glyphs. The renderer never fills unobserved spaces with invented terrain.
- Visible vertical neighbors switch the overview to offset planes. The prototype constructs the projection in Slate; it does not yet use a 3D scene camera. An always-flat option keeps explicit above/below labels.
- Outdoor rain and snow use sparse animated marks. Fog adds a restrained screen veil. Server perception remains the source of visibility; a client effect cannot create or hide authoritative content.

## Input and discoverability

| Input | Result |
| --- | --- |
| WASD | Continuous movement intention, periodically refreshed while held |
| Click terrain | Server path request |
| Ctrl-click | Facing request without movement, including when clicking an entity |
| Click a visible entity or door | Anchored action menu |
| E | Open the nearest visible entity/door action menu |
| 1–6 | Invoke the matching open contextual-menu action |
| Enter | Enter writing; while writing, submit and return to navigation |
| Shift-Enter | Insert a newline in the real multiline editor |
| Escape | Close a focused panel/menu, or leave writing with the draft intact |
| M / I / C / L | Map toggle / inventory / character / listen |

The general-action row offers Listen, Look, Smell, Wait, and Sit. Opening an interaction never auto-runs its action. Closed-door navigation and portal transitions are server rules. A transition clears held input and local viewport pan. Losing widget focus clears held movement, preventing a stuck movement key.

## Acceptance and verification

`RATW.UI.InputAndDraftRecovery` exercises navigation, typing, the real editor's newline behavior, preserved drafts, send behavior, failed-send recovery, and movement suppression. `RATW.UI.SequentialRoleplayReveal` verifies that one post completes before another starts. Native screenshots should cover local play, world overview, character, inventory, and preferences with a real server snapshot.

Review on a 1600×1000 viewport and a smaller window for clipped text, overlay placement, and visible focus. Check a second connected player to verify observed movement and indicators.

## Remaining refinements

Free-drag splitters, independent font sizing, accessible focus outlines for every painted control, full horizontal map dragging, animated projection transitions, and richer client prediction remain later work. Four selectable pane proportions are implemented. The painted contextual controls have keyboard equivalents but do not yet expose a complete screen-reader accessibility tree.
