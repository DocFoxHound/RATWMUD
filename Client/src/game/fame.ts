// Fame on the page (Docs/Design/56-fame-and-memory.md): the words the character sheet uses for what residents call a
// wolf. The deeds and nicknames are the server's.
import {bool, str, type Maybe} from './json.ts';

/** "the Lantern · first said by the stall keeper, in Upper Accord", or "(you asked folk not to use it)" once dropped. */
export function nicknameLine(n: Maybe): string {
    const where = str(n, 'town') ? `, in ${str(n, 'town')}` : '';
    const by = str(n, 'coinedBy') ? ` · first said by ${str(n, 'coinedBy').charAt(0).toLowerCase()}${str(n, 'coinedBy').slice(1)}${where}` : where;
    return `${str(n, 'text')}${bool(n, 'dropped') ? ' (you asked folk not to use it)' : by}`;
}

/** An open thread (doc 56, 9): "You promised the miller: "a pot of honey" · due in 2 days" (overdue says so). */
export function unfinishedLine(t: Maybe): string {
    const text = str(t, 'text');
    if (!t || !('days' in t)) return text;
    const days = Number(t.days);
    if (!Number.isFinite(days)) return text;
    return `${text} · ${days < 0 ? 'overdue' : days < 1 ? 'due today' : `due in ${Math.round(days)} ${Math.round(days) === 1 ? 'day' : 'days'}`}`;
}
