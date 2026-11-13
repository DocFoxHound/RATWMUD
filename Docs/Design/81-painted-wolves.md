# 81. Painted wolves

A wolf's avatar is one of three watercolour paintings (a profile and a face each), coloured with fur the player
chooses: in the creator and on the character select screen now; the other places that draw a wolf are listed last.

**The technique.** The painting is kept as brushwork; only its colours change. Each pixel's luminance goes through a
ramp of three fur colours: the painting's dark fur (luminance about 38) becomes the player's dark colour, its mid
fur (118) the mid, its light fur (212) the light, with a deeper shade below and a paler one above. The iris, found by
its amber, takes the eye colour; the nose, the mouth and the eyes' rims ("ink") stay as painted. `Client/src/ui/fur.ts`
(the ramp, the blend; no page needed) and `wolfPaint.ts` (loads the paintings, paints at the size shown, keeps each
look) repaint in a few milliseconds, so the creator's preview is live. The three paintings are the appearance's
existing `build`: `lean` (tall and lean), `average` (the stocky one; also when none is given), `heavy` (the largest).

**The eight zones:** back (the saddle and top line), sides, chest and belly, legs, tail, tail tip, head, muzzle and
cheeks; on the face, head (with the ears) and muzzle and cheeks (with the pale ruff under the chin). Each wears the
coat's three colours or three of its own. A zone map says, per pixel, which two zones meet and how much of each.

**The palette's rule:** earthy, natural, muted; no neon. `Data/Wolves/palette.json` is the one list: 34 fur colours
(blacks, greys, whites and creams, tans, browns, russets, ochres, slate and dusk blues, green-greys), 11 eye colours,
the zones and ten ready-made coats. `tools/fur_palette.py` checks it (a fur colour's channels at most 100 apart, an
eye's 160) and writes `Core/RatwFurPalette.inc` and `Client/src/ui/furPalette.generated.ts`. A wolf keeps palette ids,
never colours; an id in use is never removed or renamed. The palette is in the client-data hash.

**The data.** The appearance gains one optional field, checked in `Core/RatwAppearance` and `wire::readAppearance`,
so central's `ratw_identity` and every world refuse the same things (off the palette, incomplete, extra):

    "build": "lean", "fur": {"coat": ["russet", "tan", "cream"], "eyes": "hazel", "zones": {"legs": ["silver", "mist", "snow"]}}

`coat` is dark, mid, light; `zones` is left out when no zone has its own. The creator also writes the older fields to
match (`coat`, `gradientTint`, `eyes`, a marking for a zone that has one), so the words ("a russet wolf") and the
places not yet painted show the same wolf.

**Older wolves** keep their data: no migration. With no `fur`, `furOf` (fur.ts) paints them from what they had: the
coat colour as the mid fur with its shadow and its pale, the belly-and-legs colour on the chest and legs, markings on
the zone of the same name (socks and stockings: legs; tail tip; mask: head; bib and belly: chest; saddle and cape:
back), else the pattern (saddle, mantle: back; piebald: sides); all as nearest palette colours. Tall or slender: lean.

**New art.** `python3 tools/avatar_prep.py lean=PATH average=PATH heavy=PATH` (sources now in `Data/Wolves/sources`)
cuts each wolf from its paper (far from the paper's colour; the two largest parts clear of the edge with dark in them,
so smudges go), and writes into `Client/src/assets/wolves` the cut-out and its map (red: two zones; green: the blend;
blue: ink and iris). Zones come from hand-placed outlines (`tools/avatar_outlines.json`, in hundredths of each
cut-out) settled on the painting: each zone keeps its core, and the band between cores is given out along the fur's
own edges, then feathered. A larger copy of the same painting needs only the re-run; a different painting needs its
outlines placed again (`--grid DIR` to read coordinates, `--overlay DIR` to check). `--sheet FILE`: the palette sheet.

**Left.** The user asked for the status screen and equipment next. Still code-drawn (`ui/wolfArt.ts` through
`drawPortrait`): the character sheet, the inspection card and the equipment doll (`ui/hud/dialogs.ts`), the scene's
portraits (`hud.ts`), the fight's figures (`combat.ts`), the story panel (`story.ts`); the map's W is a glyph. Open:
one age and one species painted (species and stature are words now); claws take the legs' fur; NPCs and Atlas's
roster get the derived look only; the client has no screen to change a wolf's look after creation.
