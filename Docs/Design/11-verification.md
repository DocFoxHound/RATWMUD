# Verification strategy

## Layers

1. CMake/CTest exercises the engine-independent simulation with explicit time and deterministic scenarios. Cover continuous movement, paths around obstacles, closed-door handling, transition anchors, soft collision, sight/hearing, exploration memory, heights and weather.
2. Unreal Automation exercises runtime services, parsing, memory deadlines, storage and server authority with the installed engine. Use isolated temporary databases; never overwrite the developer's normal save for a test.
3. A separate headless server and two real game clients establish transport, owner identity, movement replication and filtered events. Headless clients verify behavior; graphical clients capture the actual Slate game UI.
4. Screenshot review checks map/text balance, token orientation, long text, contrast, context menus, character/inventory display, weather and world-map state. Images are outputs of the running game, not generated concept art.

## Memory tests

Use an injected clock or test-only simulation advancement instead of waiting an hour. Test at 3,599 seconds, then new activity, then 3,599 and 3,600 seconds after that activity. Verify no premature summary, one consolidation, persistent recall, and no duplication after restart. Test provider failure leaves pending memory intact.

## Narrative and input tests

Check Enter, Shift+Enter, Escape and draft recovery. While typing, WASD must not move the actor. Typing expires after inactivity and never transmits prose. Speech indicators expire after four seconds even if the associated prose is queued. A later IC entry waits for the previous reveal. OOC is cell-local, fully legible and excluded from social credit.

## Adversarial behavior

Try invalid coordinates including nonfinite values, hidden targets, out-of-range actions, oversized text, duplicate commands, identity changes, repeated consolidation, NPC-only XP farming and forged relationship updates. The command gateway must fail safely and continue serving legitimate commands.

## Reporting

`Docs/TEST_REPORT.md` records commands, actual outcomes, fixes and remaining untested areas. `Docs/IMPLEMENTATION_STATUS.md` maps components to evidence. `artifacts/screenshots/` contains reviewed captures. Automated success does not replace a two-person roleplay usability session.

Reference: [Epic's automation framework](https://dev.epicgames.com/documentation/unreal-engine/automation-test-framework-in-unreal-engine). Pure simulation tests intentionally run without Unreal; engine tests cover the integration boundary.
