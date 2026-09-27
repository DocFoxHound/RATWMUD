# Character roster

`roster.json` is the shared pool of NPCs that **profession slots** in every world
draw from, plus the catalog of professions. Edit it in Atlas Workshop's
**Characters** workspace (`bash tools/editor.sh`); the format is in
`Docs/EDITOR_CONTRACT.md`.

The `assignment` and `profession` fields are written only by exports: once a
character is drawn into a job they keep it, in one place, for life. Mark a
character dead or removed instead of deleting them. Their record is also the
key for their memories in each world's save.

Keep this file in version control. It is part of the game's content, like the
worlds in `Data/Worlds/`.
