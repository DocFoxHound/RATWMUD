# Character sheet and inventory

Status: implemented focused Slate panels with configurable pixel-art profiles and server-provided inventory.

## Character presentation

The map remains `W` plus its orbiting facing glyph. The character sheet provides a static standing wolf profile, description, posture, declared current state, and social progression. The creator, owned-character roster, sheet, and visible-character inspection share a generated pixel-art doll renderer and the same server-validated appearance. Equipment is not painted onto these portraits yet.

The server supplies name, description, posture, state, social level, and Social XP. `/me` can update the declared state; inspection receives only server-permitted information. The sheet shows the initial Normal tier. Gifted and Quickened access remains a later eligibility and world-lore decision, not a client level threshold.

The player's selected speaking color remains independent of coat color. The creator supplies five species, four age portraits, three statures, sex, eight natural coat colors, a blended gradient, and four adjustable marking styles. Exact age remains private to the player; inspection receives a public life stage. Scars and equipment layering remain future work. See `19-character-creation.md` for the authoritative appearance and local-account contract.

## Inventory

Inventory cards come only from the snapshot's `inventory` records. Each shows a native vector icon, readable name, equipped/carried state, and description. The prototype has icon families for packs/satchels, bowls/food, weapons, and containers. Icons never replace text, and carried items never decorate the map token.

Herbs and prepared meals now have real, persisted quantities and integer silver-penny balances. The panel allows eating a carried meal, gathering from a nearby visible finite herb patch, and opening an accessible keeper's trade panel. Quotes disappear when the keeper leaves reach or sleeps; the server revalidates every transaction. Satchel and keepsake remain prototype non-tradeable presentation items. Equip/unequip, arbitrary transfers, splitting stacks and general crafting remain future work. See `15-npc-society-economy.md`.

The sheet now displays authoritative age, strength, wisdom and base/effective dexterity. Annual rewards and bounded age-65+ sensory/movement penalties are separate from Social XP and magic eligibility. See `14-calendar-aging.md`.

## Acceptance

- The character panel uses the same authoritative name and state as the controlled entity.
- Inspecting another entity does not read client-side hidden state.
- Every inventory record has a text label and description next to the icon.
- Opening and closing either panel leaves the map and transcript state intact.
- Neither panel continues held-key movement after opening.
- Screenshots show the shared configurable profile art and a real snapshot inventory, without fabricated live activity.

The native integration screenshot review checks panel layout, long labels/descriptions, icon legibility, and clear separation between sheet artwork and map representation. Freeform painting and equipment portrait layers are not implied by the limited palette-and-pattern creator.
