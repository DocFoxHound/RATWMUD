// A world story's quests in the journal (Docs/Design/85-story-triggers-and-campaign-control.md, Phase 6): what each
// quest says (its state, its objectives ticked, struck or still to come, optional ones marked, counts, hidden ones as
// "more to find"), its trackers, and the buttons a wolf has (abandon one under way; accept or decline one offered with
// nobody to ask). The game sends only what the wolf may see; this turns it into lines and the messages the buttons send.
import {arr, bool, isObject, num, str, type Json} from './json.ts';

export type QuestLine = {text: string, tone: 'done' | 'failed' | 'open' | 'later' | 'note'};
export type QuestButton = {label: string, verb: 'quest_abandon' | 'quest_accept' | 'quest_decline', confirm?: string};

const ENDED = ['done', 'failed', 'abandoned', 'expired'];

/** The quest's state in words: "Under way", "Ready: return to Ivo Tarmarsh", "Done"... */
export function questStatus(q: Json): string {
    const status = str(q, 'status');
    switch (status) {
        case 'offered': return str(q, 'giverName') ? `Offered by ${str(q, 'giverName')}` : 'Offered to you';
        case 'active': return 'Under way';
        case 'ready': return str(q, 'giverName') && bool(q, 'handIn') ? `Ready: return to ${str(q, 'giverName')}` : 'Ready';
        case 'done': return 'Done';
        case 'failed': return 'Failed';
        case 'abandoned': return 'Abandoned';
        case 'expired': return 'Ended';
        default: return status;
    }
}

/** The quest's lines: its summary, each objective it shows, the hidden ones still to find, its deadline. */
export function questLines(q: Json, today = Number.NaN): QuestLine[] {
    const out: QuestLine[] = [];
    if (str(q, 'summary')) out.push({text: str(q, 'summary'), tone: 'note'});
    const ended = ENDED.includes(str(q, 'status'));
    for (const o of arr(q, 'objectives').filter(isObject)) {
        const goal = num(o, 'goal', 1);
        const count = goal > 1 && !bool(o, 'done') ? ` · ${Math.min(num(o, 'amount'), goal)} / ${goal}` : '';
        const optional = bool(o, 'optional') ? ' (optional)' : '';
        if (bool(o, 'failed')) out.push({text: `✗ ${str(o, 'label')}${optional}`, tone: 'failed'});
        else if (bool(o, 'done')) out.push({text: `✓ ${str(o, 'label')}${optional}`, tone: 'done'});
        else if (!ended && o.open === false) out.push({text: `○ ${str(o, 'label')}${optional} · later`, tone: 'later'});
        else out.push({text: `· ${str(o, 'label')}${optional}${count}`, tone: ended ? 'later' : 'open'});
    }
    const hidden = num(q, 'hiddenLeft');
    if (hidden > 0 && !ended) out.push({text: hidden === 1 ? 'And one more thing to find out.' : `And ${hidden} more things to find out.`, tone: 'note'});
    const deadline = q.deadline;
    if (typeof deadline === 'number' && !ended) {
        const left = Number.isFinite(today) ? deadline - today : Number.NaN;
        out.push({text: Number.isFinite(left) && left >= 0 ? `To be done within ${left < 1 ? 'the day' : `${Math.ceil(left)} days`} (by day ${Math.floor(deadline)}).`
                                                           : `To be done by day ${Math.floor(deadline)}.`, tone: 'note'});
    }
    if (ended && str(q, 'status') !== 'expired') {
        const again = q.again;
        if (typeof again === 'number') out.push({text: `It may be taken again from day ${Math.ceil(again)}.`, tone: 'note'});
    }
    return out;
}

/** The buttons the quest gives the wolf. */
export function questButtons(q: Json): QuestButton[] {
    const status = str(q, 'status');
    if (status === 'offered' && bool(q, 'accept'))
        return [{label: 'ACCEPT', verb: 'quest_accept'}, {label: 'DECLINE', verb: 'quest_decline'}];
    if (bool(q, 'abandon') && (status === 'active' || status === 'ready'))
        return [{label: 'ABANDON THE QUEST', verb: 'quest_abandon', confirm: `Put aside "${str(q, 'title')}"?`}];
    return [];
}

/** The message a button sends: the journal's storyline command, for the quest of that world story. */
export function questMessage(storyline: string, quest: string, verb: QuestButton['verb']): Json {
    return {type: 'storyline', verb, storyline, quest};
}

/** A story's trackers (its shown variables) as "label: value". */
export function trackerLines(t: Json): string[] {
    return arr(t, 'trackers').filter(isObject).map(k => {
        const v = k.value;
        const value = typeof v === 'boolean' ? (v ? 'yes' : 'no') : typeof v === 'number' ? String(Math.round(v * 100) / 100) : String(v ?? '');
        return `${str(k, 'label')}: ${value}`;
    });
}

/** A world story's notes in the wolf's journal (doc 85, Phase 9: journal.note), newest last, with the day each was written. */
export function noteLines(t: Json): string[] {
    return arr(t, 'notes').filter(isObject).map(n => `✎ ${str(n, 'text')}`);
}

/** A world story's markers on the maps (doc 85, Phase 9: story.ping): where in the realm's tiles, the label, the place. */
export interface StoryPing {wx: number; wy: number; label: string; place: string; inside: boolean; story: string}
export function storyPings(journal: unknown): StoryPing[] {
    const out: StoryPing[] = [];
    for (const t of Array.isArray(journal) ? journal.filter(isObject) : [])
        for (const p of arr(t, 'pings').filter(isObject)) {
            const wx = Number(p.wx), wy = Number(p.wy);
            if (!Number.isFinite(wx) || !Number.isFinite(wy)) continue;
            out.push({wx, wy, label: str(p, 'label'), place: str(p, 'place'), inside: bool(p, 'inside'), story: str(t, 'title')});
        }
    return out;
}
