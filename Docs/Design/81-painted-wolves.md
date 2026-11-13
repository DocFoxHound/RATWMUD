# 81. Painted wolves

A wolf's avatar is watercolour: a profile in one of three sizes and a face (one painting, shared by every wolf),
coloured with fur the player chooses: in the creator and on the character select screen now; the other places that
draw a wolf are listed last. The face and the profile are separate pictures: the creator and the other preview cards
show the face as a portrait beside the wolf's name and the profile below; the roster shows each wolf's face in its
list and the selected wolf's profile on its card.

**The technique.** The painting is kept as brushwork; only its colours change. Each pixel's luminance goes through a
ramp of three fur colours: the painting's dark fur (luminance about 38) becomes the player's dark colour, its mid
fur (118) the mid, its light fur (212) the light, with a deeper shade below and a paler one above. The iris, found by
its amber, takes the eye colour; the nose, the mouth and the eyes' rims ("ink") stay as painted. `Client/src/ui/fur.ts`
(the ramp, the blend; no page needed) and `wolfPaint.ts` (loads the paintings, paints at the size shown, keeps each
look) repaint in a few milliseconds, so the creator's preview is live.

**The three sizes** are the appearance's `build`: `small`, `medium`, `large` (the second art, 2026-11-13: three wolves
painted at one scale, 399, 486 and 582 pixels high). The three profiles are kept in one frame (the large wolf's, the
paws on one ground line), so wherever they are drawn they keep those sizes: the small wolf is smaller in the creator's
size cards and on a card. The creator has one choice, the size, and writes the stature that goes with it (`short`,
`average`, `tall`), so Core's height at the shoulder (0.88, 1, 1.12 of the species') and the words strangers see
("a tall grey wolf", "a small grey wolf") agree with the painting; beside a portrait the client says the size and
that height ("Large · 85 cm at shoulder"), never a stature of its own. `build` held the first paintings' names
before (`lean`, `average`, `heavy`): Core and the client still read them, nothing is migrated, and a wolf saved with
one keeps the painting that took its place (lean: large; heavy: medium; average: small). A wolf with no build is
painted by its stature.

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

    "build": "large", "fur": {"coat": ["russet", "tan", "cream"], "eyes": "hazel", "zones": {"legs": ["silver", "mist", "snow"]}}

`coat` is dark, mid, light; `zones` is left out when no zone has its own. The creator also writes the older fields to
match (`coat`, `gradientTint`, `eyes`, a marking for a zone that has one), so the words ("a russet wolf") and the
places not yet painted show the same wolf.

**Older wolves** keep their data: no migration. With no `fur`, `furOf` (fur.ts) paints them from what they had: the
coat colour as the mid fur with its shadow and its pale, the belly-and-legs colour on the chest and legs, markings on
the zone of the same name (socks and stockings: legs; tail tip; mask: head; bib and belly: chest; saddle and cape:
back), else the pattern (saddle, mantle: back; piebald: sides); all as nearest palette colours. Its size is its
stature's.

**New art.** The paintings are in `Data/Wolves/sources`: `face.png`, `small.png`, `medium.png`, `large.png`, each
already cut out on a transparent ground, the three profiles on canvases of one size (so their scale is the painter's).
`python3 tools/avatar_prep.py` (or `large=PATH` for one from another file) keeps each painting's own alpha, drops stray
specks (all but the largest joined part), and writes into `Client/src/assets/wolves` the picture and its map (red: two
zones; green: the blend; blue: ink and iris) as `face.png`, `face-map.png` and `<size>-profile.png`,
`<size>-profile-map.png`, the profiles set in their shared frame. Zones come from hand-placed outlines
(`tools/avatar_outlines.json`, in hundredths of each painting cropped to its wolf) settled on the painting: each zone
keeps its core (a narrow part, a leg, keeps a thinner one), and the band between cores is given out along the fur's
own edges, then feathered. Ink is the dark paint inside its outlines; an eye's outline says how dark (`below`), so
the rim is kept and the dark fur round it is not. A larger copy of the same painting needs only the re-run; a
different painting needs its outlines placed again (`--grid DIR` to read coordinates, `--overlay DIR` to check).
`--sheet FILE`: the palette sheet.

**Left.** The user asked for the status screen and equipment next. Still code-drawn (`ui/wolfArt.ts` through
`drawPortrait`): the character sheet, the inspection card and the equipment doll (`ui/hud/dialogs.ts`), the scene's
portraits (`hud.ts`), the fight's figures (`combat.ts`), the story panel (`story.ts`); the map's W is a glyph. Open:
one age and one species painted (species is words; the sheet still says the stature, which the creator keeps in
step with the size); claws take the legs' fur; NPCs and Atlas's
roster get the derived look only; the client has no screen to change a wolf's look after creation.
