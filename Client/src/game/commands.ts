// What a wolf can do without a button (the user's notes, 2026-11-14): the old ACTIONS panel under the map is gone, and
// everything it held is a slash command in the box where one writes ("/" lists them, Tab completes) and on the wheel a
// right-click opens on the ground (ui/hud/hud.ts). A wolf's own wheel is its actions, as the server sends them.
//
// Some commands are the story's own: the server reads /sit, /lay, /stand, /pose, /me and /sigh in what is written
// (Core/RatwSocialCore.cpp), and the storyteller's /narrate, /npc, /roll and /tick go to the tale (storyteller.ts).
// Those are listed for their help and sent as written; the rest are run here.
import {bool, obj, str} from './json.ts';
import type {GameState} from './state.ts';

export interface SlashCommand {
    name: string;               // Typed after the "/".
    args?: string;              // What follows it, in words ("<what you do>"); none for a bare command.
    help: string;               // One short line for the list.
    key?: string;               // Its key on the map, where there is one.
    wheel?: string;             // Its name on the ground's wheel; not on the wheel without one.
    story?: boolean;            // Read in what is written (by the server or the storyteller's tale): sent as written.
    when?: (s: GameState) => boolean;   // Only then (listed and on the wheel).
    run?: (s: GameState, arg: string) => void;
}

const self = (s: GameState) => obj(s.snapshot, 'self');
const wild = (s: GameState) => obj(s.snapshot, 'wild');
const archive = (s: GameState) => obj(self(s), 'archive');
const notFighting = (s: GameState) => !s.battle;
const lying = (s: GameState) => str(self(s), 'posture', 'standing') !== 'standing';
const storyteller = (s: GameState) => str(obj(self(s), 'storyteller'), 'state') === 'approved';
const page = (name: string) => (s: GameState) => s.activate({rect: {left: 0, top: 0, right: 0, bottom: 0}, action: name, target: ''});

export const Commands: SlashCommand[] = [
    {name: 'listen', help: 'Listen to what is around you', key: 'L', wheel: 'Listen', run: s => s.sendAction('listen')},
    {name: 'look', help: 'Look around: what is here and who', wheel: 'Look', run: s => s.sendAction('look')},
    {name: 'smell', help: 'Take in the scents here', wheel: 'Smell', run: s => s.sendAction('smell')},
    {name: 'wait', help: 'Wait a moment, doing nothing', wheel: 'Wait', run: s => s.sendAction('wait')},
    {name: 'sit', help: 'Sit down (written in the story)', wheel: 'Sit', story: true, when: notFighting, run: s => s.sendAction('sit')},
    {name: 'lay', help: 'Lie down: six hours in a bed is a full rest', wheel: 'Lie down', story: true, when: notFighting, run: s => s.sendAction('lay')},
    {name: 'stand', help: 'Stand up', wheel: 'Stand', story: true, when: s => notFighting(s) && lying(s), run: s => s.sendAction('stand')},
    {name: 'howl', help: 'Howl: call others from across the country', wheel: 'Howl', when: notFighting, run: s => s.send({type: 'howl'})},
    {name: 'groom', help: 'Groom yourself: once a game day, a little less scent for two hours', wheel: 'Groom', when: notFighting,
        run: s => s.send({type: 'groom', target: 'self'})},
    {name: 'forage', help: 'Forage what grows here', wheel: 'Forage', when: s => notFighting(s) && bool(wild(s), 'forage'), run: s => s.send({type: 'forage'})},
    {name: 'hunt', help: 'Go out after game: a hunt against what lives here', wheel: 'Hunt', when: s => notFighting(s) && bool(wild(s), 'hunt'),
        run: s => s.send({type: 'hunt'})},
    {name: 'giveup', help: 'Give up the hunt', wheel: 'Give up hunt', when: s => !!s.battle?.hunt && !s.battle.observer && !s.battle.over,
        run: s => s.send({type: 'leaveHunt'})},
    {name: 'practise', help: 'Practise at the training post', wheel: 'Practise', when: s => notFighting(s) && bool(self(s), 'trainingGround'),
        run: s => s.send({type: 'post'})},
    {name: 'board', help: "Read the town's notice board", wheel: 'Read the board', when: s => notFighting(s) && bool(self(s), 'nearBoard'),
        run: s => s.send({type: 'board', verb: 'read'})},
    {name: 'archive', help: 'Ask the keeper of records for records to sort', wheel: 'Archive work',
        when: s => notFighting(s) && !!archive(s) && !str(archive(s), 'task'), run: s => s.send({type: 'archive', verb: 'ask'})},
    {name: 'copy', help: 'Copy a page at a desk here (sit to work)', wheel: 'Copy a page',
        when: s => notFighting(s) && !!archive(s) && !str(archive(s), 'task'), run: s => s.send({type: 'archive', verb: 'ask', kind: 'copy'})},
    {name: 'perform', help: 'Sing, tell a tale or play: those resting here rest better (again to stop)', wheel: 'Perform',
        when: s => notFighting(s) && bool(self(s), 'commonRoom'), run: page('perform')},
    {name: 'pose', args: '<what you do>', help: 'Write what your wolf does', story: true},
    {name: 'me', args: '<how you are>', help: 'Set what others see you doing', story: true},
    {name: 'sigh', help: 'Sigh softly', story: true},
    {name: 'narrate', args: '<words>', help: 'Tell the scene as its storyteller', story: true, when: storyteller},
    {name: 'npc', args: '<name>: <words>', help: 'Speak as someone in your tale', story: true, when: storyteller},
    {name: 'roll', args: '<dice> [for <what>]', help: 'Roll dice for the tale', story: true, when: storyteller},
    {name: 'tick', args: '[step.objective]', help: "Tick off the tale's objective", story: true, when: storyteller},
    {name: 'character', help: 'Your character sheet', key: 'C', run: page('character')},
    {name: 'inventory', help: 'Your belongings', key: 'I', run: page('inventory')},
    {name: 'status', help: 'Your status, belongings and equipment', run: page('status')},
    {name: 'journal', help: 'Your stories under way, and what you have found out', run: s => s.send({type: 'journal'})},
    {name: 'chapter', help: 'Your Chapter', run: page('chapter_window')},
    {name: 'friends', help: 'Your friends and circles', run: page('people')},
    {name: 'settings', help: 'Settings', run: page('settings')},
    {name: 'map', help: 'The World Map (again for the local map)', key: 'M', run: s => {
        s.worldMap = !s.worldMap;
        s.contextTarget = '';
    }},
];

/** The commands one may use now. */
export function availableCommands(s: GameState): SlashCommand[] {
    return Commands.filter(c => !c.when || c.when(s));
}

/** What is typed as a command: its name (lower case) and the rest; null for anything not starting with "/". */
export function parseSlash(text: string): {name: string; arg: string} | null {
    const m = /^\/([a-z]+)(?:\s+([\s\S]*))?$/i.exec(text.trim());
    return m ? {name: m[1].toLowerCase(), arg: (m[2] ?? '').trim()} : null;
}

/** The list under the box while a command is being typed: those that begin with the word so far; none once it has
 *  its arguments or is not a command at all. */
export function slashSuggestions(s: GameState, text: string): SlashCommand[] {
    const m = /^\/([a-z]*)$/i.exec(text);
    if (!m) return [];
    const word = m[1].toLowerCase();
    return availableCommands(s).filter(c => c.name.startsWith(word));
}

/** The command a line runs here (not one the story reads), or null to send it as written. */
export function commandToRun(s: GameState, text: string): {command: SlashCommand; arg: string} | null {
    const parsed = parseSlash(text);
    if (!parsed) return null;
    const command = availableCommands(s).find(c => c.name === parsed.name);
    return command && !command.story && command.run ? {command, arg: parsed.arg} : null;
}

/** The ground's wheel (a right-click on the map, not on anyone): the commands that have a place on it now. */
export function wheelCommands(s: GameState): SlashCommand[] {
    return availableCommands(s).filter(c => c.wheel && c.run);
}

/** Runs a command by name, from the wheel or the box; false if it can't be used now. */
export function runCommand(s: GameState, name: string, arg = ''): boolean {
    const command = availableCommands(s).find(c => c.name === name);
    if (!command?.run) return false;
    command.run(s, arg);
    return true;
}
