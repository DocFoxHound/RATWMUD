// Every problem of the draft at once, each with [Go] (Docs/Design/85, Phase 3; 4.2's Problems list), moved into the
// workbench as it was built: the program's errors, what the world says (a Dungeon Master's: unknown residents, tiles
// outside a cell, what approval would refuse), the warnings the program alone can tell, and the triggers the last
// rehearsal did not reach.
import {problemList, problemPlace, type Problem, type Problems} from '../../lib/campaigns';

export function ProblemsList({found, untried, phases, go, asking}: {found: Problems | null; untried?: string[]; phases: string[];
    go?: (p: Problem) => void; asking: boolean}) {
    if (!found) return <section className="story-problems"><h4>Problems</h4><p className="hint">{asking ? 'Checking the draft…' : 'Not checked yet.'}</p></section>;
    const list = problemList(found.problems, untried, phases);
    const errors = list.filter(p => p.level === 'error').length, warnings = list.length - errors;
    return <section className="story-problems" aria-label="Problems">
        <h4>Problems · {errors ? `${errors} error${errors === 1 ? '' : 's'}` : <span className="ok">no errors</span>}{warnings ? ` · ${warnings} warning${warnings === 1 ? '' : 's'}` : ''}
            {asking ? <small className="hint"> · checking…</small> : null}</h4>
        {list.length > 0 && <ul>{list.map((p, i) => <li key={i} className={p.level}>
            <span>{p.level === 'error' ? '✖' : '⚠'}</span>
            <span>{p.text}<small>{problemPlace(p)}{p.world ? ` · from the world${p.blocks ? ': approval refuses this' : ''}` : ''}</small></span>
            {go && p.at.kind !== 'source' ? <button onClick={() => go(p)}>Go</button> : <span />}</li>)}</ul>}
        {!errors && !found.world && <p className="hint">The world's references are checked for a Dungeon Master once the draft has no error.</p>}
    </section>;
}
