// Browser-side world-story authoring. A rehearsal keeps the version it started with.
export const triggers = {decision: 'DM decision', arrival: 'Arrival at the story location', deadline: 'Deadline reached'};
export type Trigger = keyof typeof triggers;
export interface Exit {when: Trigger; to: string}
export interface StoryStep {id: string; name: string; journal: string; ending: boolean; exits: Exit[]}
export interface StoryDraft {id: string; title: string; resident: string; place: string; steps: StoryStep[]}
export interface RehearsalEvent {id: number; when: Trigger; place: string}
export interface Rehearsal {story: StoryDraft; events: RehearsalEvent[]}

export function storyId(): string {
    // Studio can be served over HTTP on the local network, where randomUUID is unavailable.
    return Array.from(crypto.getRandomValues(new Uint8Array(16)), b => b.toString(16).padStart(2, '0')).join('');
}
export function newStory(): StoryDraft {
    return {id: storyId(), title: '', resident: '', place: '', steps: [
        {id: 'start', name: 'Opening', journal: '', ending: false, exits: [{when: 'decision', to: 'end'}]},
        {id: 'end', name: 'Ending', journal: '', ending: true, exits: []}
    ]};
}
export function checkStory(story: StoryDraft) {
    const issues: string[] = [];
    if (!story.title.trim()) issues.push('Give the story a title.');
    const byId = new Map(story.steps.map(s => [s.id, s]));
    if (byId.size !== story.steps.length || !story.steps.length) return [...issues, 'Each step needs a unique ID and the story needs an opening.'];
    const reachesEnd = new Set(story.steps.filter(s => s.ending).map(s => s.id));
    for (const [i, step] of story.steps.entries()) {
        const name = step.name || 'Step ' + (i + 1);
        if (!step.name.trim()) issues.push('Name step ' + (i + 1) + '.');
        if (!step.journal.trim()) issues.push('Write the journal entry for ' + name + '.');
        if (!step.ending && !step.exits.length) issues.push(name + ' needs an exit or an ending.');
        const seen = new Set<Trigger>();
        for (const exit of step.exits) {
            if (!byId.has(exit.to)) issues.push('Choose the next step for ' + name + '.');
            if (seen.has(exit.when)) issues.push(name + ' has two exits for the same event.');
            if (exit.when === 'arrival' && !story.place) issues.push('Choose a location for the arrival in ' + name + '.');
            seen.add(exit.when);
        }
    }
    const reachable = new Set([story.steps[0].id]);
    for (const id of reachable) for (const exit of byId.get(id)?.exits ?? []) reachable.add(exit.to);
    for (let i = 0; i < story.steps.length; i++) {
        for (const step of story.steps) if (step.exits.some(e => reachesEnd.has(e.to))) reachesEnd.add(step.id);
    }
    for (const step of story.steps) {
        if (!reachable.has(step.id)) issues.push(step.name + ' is unreachable from the opening.');
        else if (!reachesEnd.has(step.id)) issues.push(step.name + ' has no path to an ending.');
    }
    return issues;
}
export function inspectRun(run: Rehearsal) {
    const [opening] = run.story.steps;
    let step = opening;
    const journal = [opening.journal], log = ['Opened ' + opening.name + '.'];
    const seen = new Set<number>();
    for (const event of run.events) {
        if (seen.has(event.id)) { log.push('Repeated event ignored.'); continue; }
        seen.add(event.id);
        const exit = step.exits.find(e => e.when === event.when);
        if (!exit || step.ending) { log.push(triggers[event.when] + ' — no matching exit from ' + step.name + '.'); continue; }
        if (event.when === 'arrival' && event.place !== run.story.place) { log.push('Arrival ignored — different location.'); continue; }
        const next = run.story.steps.find(s => s.id === exit.to);
        if (!next) { log.push('Destination step is missing.'); break; }
        log.push(triggers[event.when] + ' — ' + step.name + ' → ' + next.name + '.');
        step = next; journal.push(step.journal);
    }
    return {step, journal, log};
}

/** Files carry the editable definition. Every import gets its own library entry and rehearsal. */
export function exportStory(draft: StoryDraft) {
    return JSON.stringify({format: 'ratw-story-draft', version: 1, draft}, null, 2);
}
export function importStory(contents: string): StoryDraft {
    const file = JSON.parse(contents);
    if (file?.format !== 'ratw-story-draft' || file.version !== 1) throw Error('Choose a version 1 RATW story draft.');
    const d = file.draft;
    if (!d || ![d.title, d.place, d.resident].every(v => typeof v === 'string') || !Array.isArray(d.steps) || !d.steps.length)
        throw Error('The file is missing its story fields or opening step.');
    const steps: StoryStep[] = [];
    const seen = new Set<string>();
    for (const step of d.steps) {
        if (!step || ![step.id, step.name, step.journal].every(v => typeof v === 'string') ||
            !step.id || seen.has(step.id) || typeof step.ending !== 'boolean' || !Array.isArray(step.exits) ||
            step.exits.length > 3 || (step.ending && step.exits.length))
            throw Error('Each step needs a unique ID, journal text and valid exits.');
        seen.add(step.id);
        const exits: Exit[] = [];
        for (const e of step.exits) {
            if (!e || typeof e.to !== 'string' || typeof e.when !== 'string' || !Object.hasOwn(triggers,e.when))
                throw Error('A step has an unknown event or destination format.');
            exits.push({when:e.when, to:e.to});
        }
        steps.push({id:step.id, name:step.name, journal:step.journal, ending:step.ending, exits});
    }
    return {id:storyId(), title:d.title, place:d.place, resident:d.resident, steps};
}
