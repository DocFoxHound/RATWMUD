// The Wharf's trade credit moved by the DM (Docs/Design/73-the-wharf.md, 4.6 and 6): the live action wharf.credit, an
// amount of pennies either way (at most the world's rule wharfActionMost) and a note, queued for the game server and
// audited. No coin moves now: the credit is a limit; money comes in only as goods the beyond buys (at most
// wharfExportMost a day) and leaves only as goods landed.
import {useState} from 'react';
import {dmApi, type Target} from './api';

export function WharfCredit({target, onDone}: {target: Target; onDone: () => void}) {
    const [amount, setAmount] = useState('');
    const [note, setNote] = useState('');
    const [said, setSaid] = useState('');
    const send = () => {
        const n = Number(amount);
        if (!Number.isInteger(n) || n === 0) { setSaid('A whole number of pennies, + to inject, - to take.'); return; }
        dmApi.liveAction(target, 'wharf.credit', 'wharf', {amount: n, note: note.trim()})
            .then(r => { setSaid(`Queued (action ${r.id}).`); setAmount(''); setNote(''); onDone(); })
            .catch(e => setSaid((e as Error).message));
    };
    return <div className="dm-form">
        <label>Move the credit by <input value={amount} onChange={e => setAmount(e.target.value)} inputMode="numeric" size={7} />p</label>
        <label>Note <input value={note} onChange={e => setNote(e.target.value)} size={24} maxLength={160} /></label>
        <button onClick={send}>Move</button>
        <small className="hint">+ lets the beyond buy more of the realm's goods (money in); - lets it buy less (money out as the
            realm imports). {said}</small>
    </div>;
}
