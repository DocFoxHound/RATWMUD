// Delta snapshots, the client's side (the server's is Core/RatwSections.h). A snapshot's big parts (the cell's ground
// and heights, what the wolf can see, the maps, the doors, the satchel) change far less often than snapshots arrive.
// The server leaves out every part whose key it knows this client holds, and fill() puts the kept copy back before
// anything uses the snapshot. In the maps, an entry the client holds comes as {"$held": key}.

type Json = unknown;
type JsonObject = Record<string, Json>;

interface Section {
    parent: string;      // The object holding it: "" for the snapshot itself.
    field: string;
    name: string;        // Its name among the keys.
    entries?: boolean;   // A list of objects with an "id", each held (and sent) on its own.
    // Entries' fields that change from moment to moment: sent with each {"$held": key}, laid over the held entry.
    moving?: readonly string[];
}

export const Sections: readonly Section[] = [
    {parent: '', field: 'visibility', name: 'visibility'},
    {parent: 'cell', field: 'rows', name: 'cell.rows'},
    {parent: 'cell', field: 'heights', name: 'cell.heights'},
    {parent: '', field: 'worldMap', name: 'worldMap', entries: true},
    {parent: '', field: 'travelMap', name: 'travelMap', entries: true},
    {parent: '', field: 'doors', name: 'doors'},
    {parent: '', field: 'inventory', name: 'inventory'},
    // The wolves in view (doc 31, Phase 4): each one's details only when they change, its pose and timers every time.
    {parent: '', field: 'entities', name: 'entities', entries: true,
        moving: ['x', 'y', 'facing', 'turning', 'moving', 'postureRemaining', 'speakingRemaining']},
];

export const KeptPerSection = 6;
export const EntriesKept = 4096;

const isObject = (v: Json): v is JsonObject => typeof v === 'object' && v !== null && !Array.isArray(v);

/** The newest few versions received of each part, and the maps' entries by key. */
export class SectionCache {
    kept = new Map<string, Array<[string, Json]>>();
    entries = new Map<string, Json>();         // In insertion order: the oldest goes first.

    reset() {
        this.kept.clear();
        this.entries.clear();
    }
    keepEntry(key: string, value: Json) {
        if (this.entries.has(key)) return;
        this.entries.set(key, value);
        if (this.entries.size > EntriesKept) this.entries.delete(this.entries.keys().next().value!);
    }
}

/** `base` (a list of strings) with row edits [row, column, text] applied, or null if they don't fit it. */
export function applyRowDelta(base: Json, edits: Json): string[] | null {
    if (!Array.isArray(base) || !Array.isArray(edits) || !base.every(r => typeof r === 'string')) return null;
    const rows = (base as string[]).slice();
    for (const e of edits) {
        if (!Array.isArray(e) || e.length !== 3) return null;
        const [row, column, text] = e;
        if (!Number.isInteger(row) || !Number.isInteger(column) || typeof text !== 'string' || row < 0 || row >= rows.length || column < 0 ||
            column + text.length > rows[row].length) return null;
        rows[row] = rows[row].slice(0, column) + text + rows[row].slice(column + text.length);
    }
    return rows;
}

function parentOf(root: JsonObject, section: Section): JsonObject | null {
    if (!section.parent) return root;
    const child = root[section.parent];
    return isObject(child) ? child : null;
}

/**
 * Puts back what the server left out and keeps what it sent, leaving the snapshot as if sent whole. False if a part
 * left out isn't kept here: the client then asks for everything again and doesn't use this snapshot.
 */
export function fill(root: JsonObject, cache: SectionCache): boolean {
    const keys = root.sectionKeys;
    if (!isObject(keys)) return true;                  // Sent whole.
    let complete = true;
    for (const section of Sections) {
        const key = keys[section.name];
        if (typeof key !== 'string') continue;
        const parent = parentOf(root, section);
        if (!parent) {
            complete = false;
            continue;
        }
        let kept = cache.kept.get(section.name);
        if (!kept) cache.kept.set(section.name, kept = []);
        if (section.field in parent) {
            let value = parent[section.field];
            // Sent as changes to a version kept here (what the wolf can see, as it walks).
            if (isObject(value) && typeof value.$delta === 'string') {
                const baseKey = value.$delta;
                const base = kept.find(([k]) => k === baseKey);
                const whole = base ? applyRowDelta(base[1], value.edits) : null;
                if (!whole) {
                    complete = false;
                    continue;
                }
                parent[section.field] = value = whole;
            }
            if (section.entries && Array.isArray(value)) {
                const whole: Json[] = [];
                let referenced = false;
                for (const entry of value) {
                    if (isObject(entry) && typeof entry.$held === 'string') {
                        if (!cache.entries.has(entry.$held)) return false;
                        const {$held: _held, ...moved} = entry;
                        whole.push({...(cache.entries.get(entry.$held) as JsonObject), ...moved});
                        referenced = true;
                        continue;
                    }
                    const entryKey = isObject(entry) && typeof entry.id === 'string' ? keys[`${section.name}#${entry.id}`] : undefined;
                    if (typeof entryKey === 'string') cache.keepEntry(entryKey, entry);
                    whole.push(entry);
                }
                if (referenced) parent[section.field] = value = whole;
            }
            const index = kept.findIndex(([k]) => k === key);
            if (index >= 0) kept.splice(index, 1);
            kept.push([key, value]);
            if (kept.length > KeptPerSection) kept.shift();
            continue;
        }
        const found = kept.find(([k]) => k === key);
        if (!found) {
            complete = false;
            continue;
        }
        parent[section.field] = found[1];
    }
    delete root.sectionKeys;
    return complete;
}
