# Local accounts, character creation, and wolf appearance

Status: initial native implementation, September 21, 2026. This component
replaces the prototype silhouette and default automatic development identity
with a local account/character flow and a shared customizable standing-profile
portrait. It is **not production internet authentication**. The character ages,
body proportions, palette, and creation limits below are provisional game-design
tuning, not biological claims or additional world-bible canon.

Accounts belong to the selected world save, not a global identity service.
Run only one authority per SQLite save path; independent standalone games must
use separate `-RatwSave` paths. Password recovery and character deletion are not
implemented. To test two players together, use two clients connected to one
loopback authority, not two standalone authorities sharing a file.
Existing development characters are preserved but are not automatically claimed
by a newly registered account. They remain available through the explicit
development-identity test path; ownership migration requires a deliberate policy.
Character names are display labels and may currently repeat; generated IDs,
not names, determine ownership. Global name uniqueness needs a product decision.

## Player flow

A normal native launch opens the account screen. Register a local test account
or sign in, then select an owned character or create one. An account can hold
six characters; this development store is capped at 128 accounts. Display names
are not account credentials or stable authority identifiers: the server assigns
a distinct opaque `wolf-` character ID and checks its owner before entry.

Creation asks for a name, a whole age from 6 through 99, and appearance choices.
The live portrait preview, character selection, own character sheet, and visible
character inspection should use the same shared wolf-doll rendering component.
The player commits the choices explicitly, then selects the saved character to
enter the world. Leaving a character returns to selection; logging out returns
to the account screen. No account recovery or character deletion is implemented.

Creation is authoritative and transactionally persisted. The client cannot
choose an ID, inject attributes, grant magic eligibility, or assign earned
Social XP. New characters receive ordinary base attributes and Normal-tier
progression; selecting an older age does not claim retroactive annual rewards.
Existing age-related sense and effective-dexterity rules still apply to old
characters. The first future birthday is anchored to the shared world calendar.

The create request includes a command ID. Retrying the same ID and identical
choices returns the prior result without creating another character or repeating
startup assets; reusing that ID for different choices is rejected. Ownership,
character state, creation receipts, and finite economic startup transfers share
the same private atomic world checkpoint. Failed persistence must not acknowledge
a new character or leave its startup transfer committed only in memory.

## Appearance contract

Every entity has one independent `ratw::Appearance` value. The nested wire/save
object has exactly these nine fields:

| Field | Allowed values |
| --- | --- |
| `species` | `timber`, `maned`, `arctic`, `red`, `ethiopian` |
| `sex` | `female`, `male` |
| `stature` | `short`, `average`, `tall` |
| `pattern` | `solid`, `saddle`, `mantle`, `piebald` |
| `baseColor`, `gradientColor`, `markingColor` | Whole palette index 0–7 |
| `gradientAmount`, `patternAmount` | Finite number 0–1, inclusive |

IDs are exact lowercase ASCII identifiers. The server rejects unknown fields,
missing fields, invalid IDs, numeric strings, booleans in numeric fields,
fractional palette indices, and nonfinite or out-of-range amounts. It never
silently clamps malformed choices or turns them into a valid preset. A legacy
saved entity that entirely omits appearance receives the canonical default;
an explicitly malformed/null appearance rejects the complete restore atomically.

Default: timber, male, average, saddle; stone base, silver gradient, charcoal
marking; gradient amount 0.35 and pattern amount 0.55. Coat choices are separate
from the player's 32-color speech/typing indicator palette.

| Index | Coat color | sRGB hex |
| --- | --- | --- |
| 0 | Ivory | `#E1D9C6` |
| 1 | Silver | `#ADB3B2` |
| 2 | Ash | `#777D7B` |
| 3 | Stone | `#8E8271` |
| 4 | Sable | `#65513F` |
| 5 | Charcoal | `#303534` |
| 6 | Rust | `#A26843` |
| 7 | Sand | `#BEAA84` |

The constrained, muted palette supports natural-looking coats without replacing
the wolf's shading with a flat bright fill. Gradients and markings are blended
over the selected standing-profile art. `solid` disables the pattern mask, not
the optional coat gradient. Selecting sex does not automatically change size,
attributes, or equipment. Sex-specific visual differentiation remains limited;
do not promise anatomy changes that the current shared art does not depict.

## Age and descriptive stature

Age is an authoritative entity field, not part of appearance. It advances with
the same calendar and birthday system used by existing NPCs and players.
Creation is restricted to 6–99; restored characters may continue beyond 99 under
the pre-existing bounded aging rules rather than becoming unloadable.

| Life stage | Age | Portrait/height design factor |
| --- | --- | --- |
| Young | 6–12 | 0.65 |
| Adolescent | 13–17 | 0.87 |
| Adult | 18–64 | 1.00 |
| Old | 65+ | 0.96 |

The portable stage helper also handles legacy ages below six as young. Stage
labels are `young`, `adolescent`, `adult`, and `old`. The sheet's descriptive
shoulder height is computed from provisional average-adult baselines: timber
76 cm, maned 90 cm, arctic 72 cm, red 66 cm, Ethiopian 60 cm; multiplied by
short/average/tall factors 0.88/1/1.12 and the stage factor above. This value is a
portrait description, not a movement/collision scale or stat modifier. Portrait
composition uses authored stage frames and stature scaling; it is not a literal
centimeter-calibrated view of the game world.

Appearance choices never confer strength, speed, stealth, hearing, vision,
stamina, collision size, combat reach, or Social XP. The existing calendar/aging
system remains responsible for future birthday rewards and age-related penalties.
Species balance, starting-age balance, and any future ancestry mechanics need
separate explicit decisions; they must not enter unnoticed through the art model.

## Shared portrait, not a map avatar

`SRatwWolfDoll` consumes the same appearance object and authoritative age wherever
a portrait is required. Five shipped transparent RGBA8 PNG atlases live under
`Data/Portraits/{species}.png`. Each has four equal quadrants in this order:
top-left young, top-right adolescent, bottom-left adult, bottom-right old.
Only fixed validated species names resolve these assets; user-controlled file
paths and arbitrary remote images are not accepted. The renderer preserves
alpha and source shading while applying coat gradients and pattern masks.

The portrait is static and quadrupedal. It does not animate a roleplay pose or
dictate an action a player imagines. Gear overlays, scars, additional markings,
user-uploaded art, arbitrary mesh editing, and a general equipment preview remain
future work. Inventory icons remain independent and do not imply that a pictured
item is equipped.

The local map remains the upright `W` plus its orbiting `>` and existing small
collision footprint. Species, stature, age art, and coat colors do not replace
that representation. Speech/typing indicators and their selected colors remain
independent. This keeps descriptive detail in optional inspection without taking
over the imagination-led map and roleplay feed.

## Perception and privacy

The own-character snapshot and owner-only character list include the saved
appearance and exact age. Sight-filtered visible entities expose appearance,
public life stage, and descriptive shoulder height, but not exact age or private
attributes. Inspection can only render what the server authorizes as visible.
Clients may choose a representative age for a public stage to select its portrait
frame; that representative is presentation only, not a new inferred exact age.

Hidden actors do not enter the visible entity list. Hearing a voice or sensing a
direction never supplies its appearance, breed, sex, exact age, coat, location,
portrait asset, or an inspectable hidden ID. Guessing an ID cannot bypass the
server's inspection visibility check. Owner/account credential data never enter
public snapshots, DM/player narrative events, or another owner's character list.

## Trusted-local authentication boundary

Registration/login and character selection are the normal launch flow, not an
opt-in feature. The old `hello` development-identity path requires the explicit
`-RatwDevIdentity` flag on authority/client test sessions. It is not a fallback
when authentication fails, and `-RatwDevTools` alone does not enable it.

The native game transport is currently unencrypted. Both client and authority
restrict account credentials to standalone or validated loopback connections;
remote account access is refused even with development tools enabled. Use
unique, disposable test passwords on this trusted computer, never credentials
reused for other services. Local does not imply encrypted transport.

Usernames are normalized lowercase, 3–32 characters, starting with a letter and
then letters, digits, underscore, or hyphen. Passwords are 12–128 UTF-8 bytes
without control characters. The private checkpoint stores a 32-byte random salt
and 32-byte PBKDF2-HMAC-SHA256 verifier at 600,000 iterations, not plaintext
passwords. Authentication attempts are bounded per connection and globally;
synchronous password derivation is a deliberate local-prototype limitation.
The private SQLite checkpoint still needs OS access protection and safe backups;
hashing does not make copied password verifiers harmless.

Before internet deployment: design authenticated encrypted transport/TLS,
session expiry/revocation, secure credential handling across transport and logs,
asynchronous resource-bounded password work, abuse throttling, recovery policy,
backup/privacy controls, and operational account administration. Do not remove
the remote guard simply to allow external clients to register. Storykeeper DM
authority remains separate from character accounts and does not become a player
role or an account toggle.

## Verification and outstanding decisions

Portable tests cover every specified appearance combination, normalized bounds,
nonfinite values, stable palette/stage helpers, no gameplay bonuses, independent
player/NPC save roundtrips, atomic rejection, hidden-portrait filtering, and
birthday/restart stability. Native Automation covers strict actual JSON types,
legacy omission versus malformed presence, embedded-NUL identifiers, unchanged
outputs on failed parsing, public exact-age privacy, and all five shipped atlas
formats/quadrants. Real-window checks cover the shared preview, selection,
sheet/inspection rendering, readability, and the unchanged map token.

Open decisions: final age bands and proportions; whether starting age affects
future eligibility; richer sex/body differentiation; appearance edits after
creation; equipment/scar layering; account recovery and character retirement;
whether social progression ultimately belongs to an account or character.
The present slice preserves existing character-scoped progression and does not
quietly create an account-wide reward system.
