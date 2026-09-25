# Character creator verification

September 21, 2026 · Linux x64 · Unreal Engine 5.8.2.

## Implemented slice

Native account registration/login, six owned character slots, live creator and
review, selected-character entry/leave, durable appearance, and the same wolf
portrait in the roster, own sheet and another player's authorized inspection.
Five generated species atlases contain twenty age dolls; three stature choices,
eight natural coat colors, gradient strength and four marking patterns are
applied at runtime. Sex is descriptive rather than a separate painting. Art is
sheet-only; map glyphs are unchanged. Source art and exact prompt set are in
`Data/Portraits/README.md`; generation used the built-in image tool.

## Executed verification

- Portable Release: 12/12 suites passed, including 437 appearance/owned-wolf
  checks after the economy-ID integration fix.
- Full Clang ASAN/UBSAN: 12/12 passed before that final ID integration fix;
  targeted society, director and appearance sanitizer suites passed afterward.
- Native Unreal Automation: 33/33 passed. Includes strict appearance JSON,
  malformed/legacy checkpoint handling, sight filtering, five shipped RGBA
  atlas decodes, account hashes/ownership/rate limits, and creator/UI tests.
- Real native process flow passed in a disposable save: registration, invalid
  creation rejection, all species and age frames represented in six characters,
  idempotent create retry, seventh-slot rejection, entry/leave, cross-account
  ownership rejection, wrong-password rejection and re-login.
- Authority restart plus independent clients passed: saved avatar recovery,
  other-player inspection and duplicate-active-character rejection. Inspection
  carries public life stage but not another character's exact age.
- Private SQLite checkpoint retained seven characters across two test accounts,
  contained no plaintext fixture password, and had owner-only file permissions.
- Final UI contrast/height revision re-passed 33/33 native Automation tests and
  the complete real-process character flow. Legacy opt-in two-client networking
  also passed movement plus reciprocal IC/local-OOC delivery.
- Standalone Linux Development package build/cook/stage/archive succeeded;
  all five portrait PNG atlases are bundled as runtime files.
- Packaged real-process flow also passed in full: creation/ownership/retry/slot
  checks, private checkpoint inspection, server restart, two-client inspection
  and duplicate-active-character rejection. Packaged captures 33–39 were saved;
  the live creator screenshot was visually reviewed with its bundled art.

Reproduce with `bash tools/build.sh`, `bash tools/test-engine.sh` and
`python3 tools/character_smoke.py`. The latter uses real Unreal processes,
fresh test saves and no development-identity bypass. `--headless` omits only
screenshots; `--packaged` targets the built Linux package. No paid NPC provider
calls are needed for these tests.

Evidence: `artifacts/logs/characters-build.log`,
`artifacts/logs/characters-automation.log`,
`artifacts/logs/characters-native-smoke.log`,
`artifacts/screenshots/characters-native-summary.json` and captures 33–39.
Screenshots are actual rendered native viewports, not generated UI mockups.
The packaged counterparts are `artifacts/logs/characters-package.log`,
`artifacts/logs/characters-packaged-smoke.log`, and
`artifacts/packaged-evidence/characters-packaged-summary.json` plus captures 33–39.

## Review-driven corrections

Creation retries retain a stable request ID; the server stores a durable receipt
and rejects changed payload reuse. Generated character IDs now work through all
finite-economy grant/trade/gather/eat/restore paths. Creation refuses missing
economy accounts, ownership validation works in both directions, and corrupted
appearance records fail atomically. Decoder-native BGRA ordering is normalized
before applying coat colors. Screenshots prompted correction of dark native
button brushes that obscured the intended swatches and selected-roster text.

## Boundaries

This is a trusted-local prototype, not production internet authentication.
Credentials are restricted on both client and authority to standalone/loopback;
public encrypted transport, recovery, deletion/retirement, global accounts and
load/abuse evaluation remain future work. One authority owns each save path.
No existing valuable player save was used for smoke tests. Password hashing is
synchronous but request-bounded. Development identity entry is explicit opt-in
on both endpoints and is used only by legacy regression scripts.

Age bands, starting ages and six slots are provisional. Equipment/scar layers,
eye-color controls, post-creation cosmetic editing and unrestricted coat painting
are not implemented. Generated art remains a first-pass art set, not biological
reference material. Natural death and combat remain separate planned components.
