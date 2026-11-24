// Fame on the page (Docs/Design/56-fame-and-memory.md): the words the character sheet uses for what residents call a
// wolf. The deeds and nicknames are the server's.
import {bool, num, str, type Maybe} from './json.ts';

/** "the Lantern · first said by the stall keeper, in Ridgemere", or "(you asked folk not to use it)" once dropped. */
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

/** A title a story named the wolf by (doc 86, C4): "the Lamp of the Ford · known in the realm", or "Warden of the Weir ·
 *  known in Greyfen · 5 days left". */
export function titleLine(t: Maybe): string {
    const where = str(t, 'known') === 'town' && str(t, 'town') ? `known in ${str(t, 'town')}` : 'known in the realm';
    const days = num(t, 'days', -1);
    const left = days < 0 ? '' : days < 1 ? ' · its last day' : ` · ${Math.round(days)} ${Math.round(days) === 1 ? 'day' : 'days'} left`;
    return `${str(t, 'title')} · ${where}${left}`;
}
