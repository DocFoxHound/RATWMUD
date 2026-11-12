// The LIVE tab's words for the world's events (Docs/Design/34, 1.1): who did what, and where, by name. An id (a
// resident's "tn_lf_rowan_downridge", a character's) is never shown: a name if the page knows one, else nothing.

export interface EventWords { kind: string; actor: string; target: string; cell: string; detail: string }

/** Whether a string is an id rather than a name: lowercase words joined by underscores, or a long hex or uuid. */
export const looksLikeId = (s: string) => /^[a-z0-9]+(?:_[a-z0-9]+)+$/.test(s) || /^[0-9a-f-]{16,}$/i.test(s);

/** "crier (Rowan Downridge) in The Sunfields: Hear ye!" */
export function eventLine(e: EventWords, placeName: (cell: string) => string, nameOf: (id: string) => string | undefined = () => undefined): string {
    const named = (id: string) => {
        if (!id) return '';
        const name = nameOf(id);
        if (name && name !== id) return name;
        return looksLikeId(id) ? '' : id;
    };
    const who = [named(e.actor), named(e.target)].filter(Boolean).join(' → ');
    const place = placeName(e.cell);
    return `${e.kind}${who ? ` (${who})` : ''}${place && !looksLikeId(place) ? ` in ${place}` : ''}${e.detail ? `: ${e.detail}` : ''}`;
}
