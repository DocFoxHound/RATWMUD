# Character sheet and inventory

Status: implemented focused Slate panels with prototype profile art and server-provided inventory.

## Character presentation

The map remains `W` plus its orbiting facing glyph. The character sheet provides room for a static standing wolf profile, description, posture, declared current state, and social progression. The current profile is an original vector-drawn wolf silhouette with a satchel. Its caption explicitly identifies its coat and equipment as prototype art; it must not be mistaken for fully implemented appearance customization or an authoritative equipment preview.

The server supplies name, description, posture, state, social level, and Social XP. `/me` can update the declared state; inspection receives only server-permitted information. The sheet shows the initial Normal tier. Gifted and Quickened access remains a later eligibility and world-lore decision, not a client level threshold.

The player's selected speaking color is editable through the preferences panel. Coat colors, markings, scars, build, age, and equipment layering are the intended appearance controls but are not part of the current slice's profile editor. Further art should use quadrupedal wolf anatomy and the setting's nonhuman equipment conventions.

## Inventory

Inventory cards come only from the snapshot's `inventory` records. Each shows a native vector icon, readable name, equipped/carried state, and description. The prototype has icon families for packs/satchels, bowls/food, weapons, and containers. Icons never replace text, and carried items never decorate the map token.

The panel is read-only in this first slice. Equip/unequip, splitting stacks, transferring items, capacity constraints, and loss/death ownership rules need gameplay decisions and server actions before controls are added.

## Acceptance

- The character panel uses the same authoritative name and state as the controlled entity.
- Inspecting another entity does not read client-side hidden state.
- Every inventory record has a text label and description next to the icon.
- Opening and closing either panel leaves the map and transcript state intact.
- Neither panel continues held-key movement after opening.
- Screenshots explicitly show prototype profile art and a real snapshot inventory, without fabricated live activity.

The native integration screenshot review checks panel layout, long labels/descriptions, icon legibility, and clear separation between sheet artwork and map representation. Full custom coat authoring is a documented follow-up rather than silently implied as complete.
