// The Story Maker's guide in the Editor (Docs/Design/85-story-triggers-and-campaign-control.md, Phase 10): the same file as
// Docs/Story-Maker-guide.md, read as the little Markdown it is written in (headings, paragraphs, lists, tables, code, inline
// code, bold, italic, links, an anchor before a reference entry), cut into units (a section's text, or one reference entry)
// so it can be searched. Pure: no React here.

export type Inline = {t: 'text' | 'code' | 'b' | 'i'; s: string} | {t: 'a'; s: string; href: string};
export type Block =
    | {type: 'h'; level: number; text: string; id: string}
    | {type: 'p'; text: string; anchor?: string}
    | {type: 'list'; ordered: boolean; items: string[]}
    | {type: 'code'; lang: string; text: string}
    | {type: 'table'; head: string[]; rows: string[][]};

/** The id a heading is linked by (GitHub's way, as the guide's contents use it). */
export function slugOf(text: string): string {
    return text.toLowerCase().replace(/[`*]/g, '').replace(/[^\p{L}\p{N}\s-]/gu, '').trim().replace(/\s/g, '-');
}

const cells = (line: string) => line.trim().replace(/^\||\|$/g, '').split(/(?<!\\)\|/).map(c => c.trim());

export function parseGuide(md: string): Block[] {
    const lines = md.replace(/\r/g, '').split('\n');
    const out: Block[] = [];
    let i = 0;
    while (i < lines.length) {
        const line = lines[i];
        if (!line.trim() || /^<!--.*-->$/.test(line.trim())) { i++; continue; }
        const fence = line.match(/^```(\w*)/);
        if (fence) {
            const body: string[] = [];
            for (i++; i < lines.length && !lines[i].startsWith('```'); i++) body.push(lines[i]);
            out.push({type: 'code', lang: fence[1], text: body.join('\n')});
            i++;
            continue;
        }
        const h = line.match(/^(#{1,6})\s+(.*)$/);
        if (h) { out.push({type: 'h', level: h[1].length, text: h[2].trim(), id: slugOf(h[2])}); i++; continue; }
        if (line.trim().startsWith('|') && i + 1 < lines.length && /^\s*\|[\s:|-]+\|\s*$/.test(lines[i + 1])) {
            const head = cells(line);
            const rows: string[][] = [];
            for (i += 2; i < lines.length && lines[i].trim().startsWith('|'); i++) rows.push(cells(lines[i]));
            out.push({type: 'table', head, rows});
            continue;
        }
        const item = line.match(/^(\s*)([-*]|\d+\.)\s+(.*)$/);
        if (item) {
            const ordered = /\d/.test(item[2]);
            const items: string[] = [];
            while (i < lines.length) {
                const m = lines[i].match(/^(\s*)([-*]|\d+\.)\s+(.*)$/);
                if (m && m[1].length <= 1) { items.push(m[3]); i++; continue; }
                if (lines[i].trim() && /^\s{2,}\S/.test(lines[i]) && items.length) { items[items.length - 1] += ' ' + lines[i].trim(); i++; continue; }
                break;
            }
            out.push({type: 'list', ordered, items});
            continue;
        }
        const para: string[] = [];
        while (i < lines.length && lines[i].trim() && !/^(#{1,6}\s|```|\s*([-*]|\d+\.)\s)/.test(lines[i])
            && !(lines[i].trim().startsWith('|') && i + 1 < lines.length && /^\s*\|[\s:|-]+\|\s*$/.test(lines[i + 1]))) para.push(lines[i++].trim());
        let text = para.join(' ');
        const anchor = text.match(/^<a id="([^"]+)"><\/a>/);
        if (anchor) text = text.slice(anchor[0].length);
        out.push({type: 'p', text, ...(anchor ? {anchor: anchor[1]} : {})});
    }
    return out;
}

/** A line's inline parts: `code`, **bold**, *italic*, [text](href); the rest as text. */
export function inline(s: string): Inline[] {
    const out: Inline[] = [];
    const re = /`([^`]+)`|\*\*(.+?)\*\*|\*([^*\s][^*]*?)\*|\[([^\]]+)\]\(([^)\s]+)\)/g;
    let at = 0;
    for (let m = re.exec(s); m; m = re.exec(s)) {
        if (m.index > at) out.push({t: 'text', s: s.slice(at, m.index)});
        if (m[1] !== undefined) out.push({t: 'code', s: m[1]});
        else if (m[2] !== undefined) out.push({t: 'b', s: m[2]});
        else if (m[3] !== undefined) out.push({t: 'i', s: m[3]});
        else out.push({t: 'a', s: m[4], href: m[5]});
        at = m.index + m[0].length;
    }
    if (at < s.length) out.push({t: 'text', s: s.slice(at)});
    return out;
}

/** A searchable piece: a heading and the text under it, or one reference entry (its anchor and blocks). */
export interface Unit {id: string; title: string; section: string; part: string; blocks: Block[]; words: string}

const plain = (b: Block) => b.type === 'h' || b.type === 'p' ? b.text : b.type === 'list' ? b.items.join(' ') : b.type === 'code' ? b.text
    : [...b.head, ...b.rows.flat()].join(' ');

export function unitsOf(blocks: Block[]): Unit[] {
    const units: Unit[] = [];
    let section = '', part = '', current: Unit | null = null;
    const start = (id: string, title: string, first: Block[]) => {
        current = {id, title, section, part, blocks: first, words: ''};
        units.push(current);
    };
    for (const b of blocks) {
        if (b.type === 'h') {
            if (b.level <= 2) { section = b.text; part = ''; } else part = b.text;
            start(b.id, b.text, [b]);
        } else if (b.type === 'p' && b.anchor) {
            const title = b.text.replace(/\*\*/g, '').split(':')[0].replace(/`/g, '');
            start(b.anchor, title, [b]);
        } else if (current) (current as Unit).blocks.push(b);
        else start('top', 'The Story Maker', [b]);
    }
    for (const u of units) u.words = u.blocks.map(plain).join(' ').toLowerCase();
    return units;
}

/** The units whose words hold every word searched for (all of them when nothing is). */
export function searchGuide(units: Unit[], query: string): Unit[] {
    const words = query.toLowerCase().split(/\s+/).filter(Boolean);
    if (!words.length) return units;
    return units.filter(u => words.every(w => u.words.includes(w) || u.id.includes(w)));
}

/** The contents: the guide's own sections, and the reference's parts under it. */
export function contentsOf(blocks: Block[]): {id: string; text: string; level: number}[] {
    return blocks.flatMap(b => b.type === 'h' && b.level >= 2 && b.level <= 4 ? [{id: b.id, text: b.text, level: b.level}] : []);
}
