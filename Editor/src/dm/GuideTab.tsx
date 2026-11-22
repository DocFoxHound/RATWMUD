// The Story Maker's guide for admins (Docs/Design/85-story-triggers-and-campaign-control.md, Phase 10): how to make a small
// quest, how to build a campaign of chapters, flow, variables and scopes, quests and rewards, the console and steering,
// moving a run onto a new version, permissions, what the game undoes when a run ends, troubleshooting with Explain and Why
// not?, and every event, condition, action and effect explained from the trigger catalogue with an example. The same file as
// Docs/Story-Maker-guide.md (its reference part made by tools/story_guide.py), searchable here.
import {Fragment, useEffect, useMemo, useRef, useState, type ReactNode} from 'react';
import guideText from '../../../Docs/Story-Maker-guide.md?raw';
import {contentsOf, inline, parseGuide, searchGuide, unitsOf, type Block} from '../lib/guide';

function Line({s, go}: {s: string; go: (id: string) => void}) {
    return <>{inline(s).map((p, i) => p.t === 'code' ? <code key={i}>{p.s}</code> : p.t === 'b' ? <b key={i}><Line s={p.s} go={go} /></b>
        : p.t === 'i' ? <i key={i}>{p.s}</i> : p.t === 'a' ? (p.href.startsWith('#')
            // (A place in the guide: a button, never a link the page follows; anything else is shown as words.)
            ? <button key={i} type="button" className="link-button" onClick={() => go(p.href.slice(1))}>{p.s}</button>
            : <Fragment key={i}>{p.s} (<code>{p.href}</code>)</Fragment>)
        : <Fragment key={i}>{p.s}</Fragment>)}</>;
}

function Blocks({blocks, go}: {blocks: Block[]; go: (id: string) => void}) {
    return <>{blocks.map((b, i): ReactNode => {
        if (b.type === 'h') {
            const H = (`h${Math.min(6, b.level + 1)}`) as 'h2';
            return <H key={i} id={'guide-' + b.id}><Line s={b.text} go={go} /></H>;
        }
        if (b.type === 'p') return <p key={i} id={b.anchor ? 'guide-' + b.anchor : undefined} className={b.anchor ? 'guide-entry' : undefined}><Line s={b.text} go={go} /></p>;
        if (b.type === 'list') { const L = b.ordered ? 'ol' : 'ul'; return <L key={i}>{b.items.map((t, n) => <li key={n}><Line s={t} go={go} /></li>)}</L>; }
        if (b.type === 'code') return <pre key={i} className="guide-code"><code>{b.text}</code></pre>;
        return <table key={i} className="dm-table guide-table"><thead><tr>{b.head.map((h, n) => <th key={n}><Line s={h} go={go} /></th>)}</tr></thead>
            <tbody>{b.rows.map((r, n) => <tr key={n}>{r.map((c, k) => <td key={k}><Line s={c} go={go} /></td>)}</tr>)}</tbody></table>;
    })}</>;
}

export function GuideTab({focus, text = guideText, initialQuery = ''}: {focus?: string; text?: string; initialQuery?: string}) {
    const blocks = useMemo(() => parseGuide(text), [text]);
    const units = useMemo(() => unitsOf(blocks), [blocks]);
    const contents = useMemo(() => contentsOf(blocks), [blocks]);
    const [query, setQuery] = useState(initialQuery);
    const found = useMemo(() => searchGuide(units, query), [units, query]);
    const page = useRef<HTMLDivElement>(null);
    const go = (id: string) => {
        setQuery('');
        requestAnimationFrame(() => document.getElementById('guide-' + id)?.scrollIntoView({block: 'start'}));
    };
    useEffect(() => { if (focus) go(focus); }, [focus]); // eslint-disable-line react-hooks/exhaustive-deps
    const reference = contents.findIndex(c => c.text.startsWith('Reference'));
    return <div className="dm-guide">
        <nav className="guide-contents" aria-label="The guide's contents">
            <input type="search" placeholder="Search the guide (story.start, a purse, Why not…)" aria-label="Search the guide" value={query}
                onChange={e => setQuery(e.target.value)} />
            <ol>{contents.map((c, i) => <li key={c.id} className={`l${c.level}${reference >= 0 && i > reference && c.level > 2 ? ' ref' : ''}`}>
                <button type="button" className="link-button" onClick={() => go(c.id)}>{c.text}</button></li>)}</ol>
        </nav>
        <div className="guide-page" ref={page}>
            {query.trim() ? <>
                <p className="hint">{found.length} part{found.length === 1 ? '' : 's'} of the guide mention{found.length === 1 ? 's' : ''} “{query.trim()}”.
                    <button className="link-button" onClick={() => setQuery('')}>Show the whole guide</button></p>
                {found.slice(0, 80).map(u => <section key={u.id + u.section} className="guide-found">
                    <small className="hint">{u.section}{u.part ? ' › ' + u.part : ''}</small>
                    <Blocks blocks={u.blocks} go={go} /></section>)}
                {found.length > 80 && <p className="hint">{found.length - 80} more: type more words to narrow.</p>}
            </> : <Blocks blocks={blocks} go={go} />}
        </div>
    </div>;
}
