// The story column: the place, the transcript and the composer. The main focus of the screen, as it always was; now
// real text, so wrapping, selecting and scrolling are the browser's.
import {css} from '../color.ts';
import {Muted, speakingColor} from '../theme.ts';
import {keptByMyScene, type GameState, type Post} from '../../game/state.ts';
import {drawPortrait, type Portraits} from '../portrait.ts';
import {inParty, withPlayers} from '../../game/party.ts';
import {button, el, setClass, setText, show} from './dom.ts';
import {arr, bool, isObject, num, obj, str} from '../../game/json.ts';
import {sceneNeedsLabel, sceneQuietLabel} from '../../game/labels.ts';

const MaxShown = 300;

export class StoryPanel {
    readonly root: HTMLElement;
    readonly textarea: HTMLTextAreaElement;
    private statusChip: HTMLButtonElement;   // One's status (doc 50): in character, looking for a scene, out of character.
    readonly targets: HTMLElement;          // Talk targets (phase 4) sit above the composer.
    private s: GameState;
    private place: HTMLElement;
    private scene: HTMLElement;
    private feedLabel: HTMLElement;
    private sceneBar: HTMLElement;
    private sceneKey = '';
    private feed: HTMLElement;
    private empty: HTMLElement;
    private ic: HTMLButtonElement;
    private ooc: HTMLButtonElement;
    private party: HTMLButtonElement;
    private partyOoc: HTMLButtonElement;
    private chapter: HTMLButtonElement;
    private chapterOoc: HTMLButtonElement;
    private private_: HTMLButtonElement;
    private circleTabs: HTMLElement;
    private sceneFilter: HTMLButtonElement;
    private filtered = false;
    private circleKey = '';
    private mode: HTMLElement;
    private queued: HTMLElement;
    private volume: HTMLButtonElement;
    private hint: HTMLElement;
    private recover: HTMLButtonElement;
    private shown = new Map<Post, {row: HTMLElement; text: HTMLElement; revealed: number; version?: number; lines?: HTMLElement}>();
    private channel = '';
    private stick = true;                   // Following the newest post (until the reader scrolls up).

    private portraits?: Portraits;

    constructor(parent: HTMLElement, state: GameState, portraits?: Portraits) {
        this.s = state;
        this.portraits = portraits;
        this.root = el('section', 'story', parent);
        const head = el('div', 'story-head', this.root);
        el('span', 'label gold', head, 'THE STORY');
        const tabs = el('div', 'tabs', head);
        this.ic = button('IN WORLD', 'tab', tabs, () => state.activate({rect: noRect, action: 'ic', target: ''}));
        // The party's two (doc 32): in character, heard as speech is; out of character, at any distance.
        this.party = button('PARTY', 'tab', tabs, () => state.activate({rect: noRect, action: 'party', target: ''}));
        this.partyOoc = button('PARTY OOC', 'tab', tabs, () => state.activate({rect: noRect, action: 'partyooc', target: ''}));
        this.chapter = button('CHAPTER', 'tab', tabs, () => state.activate({rect: noRect, action: 'chapter', target: ''}));
        this.chapterOoc = button('CHAPTER OOC', 'tab', tabs, () => state.activate({rect: noRect, action: 'chapterooc', target: ''}));
        this.ooc = button('LOCAL OOC', 'tab', tabs, () => state.activate({rect: noRect, action: 'ooc', target: ''}));
        // Out of character, to friends anywhere (doc 50): not "tells", which are a Gifted wolf's.
        this.private_ = button('PRIVATE', 'tab', tabs, () => state.activate({rect: noRect, action: 'private', target: ''}));
        this.private_.title = 'Private messages with your friends, out of character, wherever they are';
        this.circleTabs = el('span', 'circle-tabs', tabs);   // One tab a circle (doc 50).
        this.place = el('h2', 'place', this.root);
        this.scene = el('p', 'scene', this.root);
        this.sceneBar = el('div', 'scene-bar', this.root);
        const feedHead = el('div', 'feed-head', this.root);
        this.feedLabel = el('div', 'label muted feed-label', feedHead);
        // One's own scenes only (doc 51, §7): their lines, one's own, the world's, and anything meant for one.
        this.sceneFilter = button('MY SCENE ONLY', 'small scene-filter', feedHead, () => state.activate({rect: noRect, action: 'my_scene', target: ''}));
        this.sceneFilter.title = 'Show only your scenes\' lines (and your own, the world\'s, and anything said to you)';
        this.feed = el('div', 'feed', this.root);
        this.empty = el('p', 'feed-empty', this.feed,
            'The scene is yours to enter. Listen to the room, approach someone, or press Enter to begin a conversation.');
        this.feed.addEventListener('scroll', () => {
            this.stick = this.feed.scrollHeight - this.feed.scrollTop - this.feed.clientHeight < 24;
        });
        const bar = el('div', 'composer-bar', this.root);
        this.mode = el('span', 'label', bar);
        this.queued = el('span', 'label gold', bar);
        this.recover = button('RECOVER PRIOR POST', 'small', bar, () => state.activate({rect: noRect, action: 'recover', target: ''}));
        // One's status, a click away (doc 50): in character, looking for a scene, out of character, round again.
        this.statusChip = button('IN CHARACTER', 'small status-chip', bar, () => {
            const now = str(state.profileOwn, 'status', 'ic');
            state.sendProfile('status', {value: now === 'ic' ? 'lfs' : now === 'lfs' ? 'ooc' : 'ic'});
        });
        this.statusChip.title = 'Your status, shown to others in In Sight and on hover: in character, looking for a scene, or out of character.';
        this.targets = el('div', 'talk-targets', this.root);
        const row = el('div', 'composer-row', this.root);
        this.volume = button('SPEAK', 'volume', row, () => state.activate({rect: noRect, action: 'volume', target: ''}));
        this.textarea = el('textarea', 'composer', row);
        this.textarea.placeholder = 'Press Enter to write your part in the story…';
        this.textarea.readOnly = true;
        this.textarea.spellcheck = true;
        this.hint = el('div', 'label muted composer-hint', this.root);
    }

    update() {
        const s = this.s;
        setText(this.place, s.cellName);
        const status = str(s.profileOwn, 'status', 'ic');
        setText(this.statusChip, status === 'lfs' ? 'LOOKING FOR A SCENE' : status === 'ooc' ? 'OUT OF CHARACTER' : status === 'storyteller' ? 'STORYTELLER' : 'IN CHARACTER');
        setClass(this.statusChip, 'lfs', status === 'lfs');
        setText(this.scene, s.sceneDescription || (s.selfId ? 'No scene description has been authored yet.' : 'Connecting to the persistent world…'));
        setClass(this.ic, 'active', s.channel === 'ic');
        setClass(this.ooc, 'active', s.channel === 'ooc');
        setClass(this.party, 'active', s.channel === 'party');
        setClass(this.partyOoc, 'active', s.channel === 'partyooc');
        const grouped = inParty(s.party);
        show(this.party, grouped);
        show(this.partyOoc, withPlayers(s.party));       // (Residents travelling along hear only what is said aloud.)
        setClass(this.chapter, 'active', s.channel === 'chapter');
        setClass(this.chapterOoc, 'active', s.channel === 'chapterooc');
        show(this.chapter, s.inChapter());
        show(this.chapterOoc, s.inChapter());
        setClass(this.private_, 'active', s.channel === 'private');
        show(this.private_, s.friends.length > 0 || s.channel === 'private' || s.posts.some(p => p.channel === 'private'));
        setText(this.private_, s.unreadPrivate > 0 ? `PRIVATE · ${s.unreadPrivate}` : 'PRIVATE');
        setClass(this.private_, 'unread', s.unreadPrivate > 0);
        this.updateCircleTabs();
        const circle = s.channel.startsWith('circle:') ? s.circles.find(c => `circle:${str(c, 'id')}` === s.channel) : undefined;
        setText(this.feedLabel, circle ? `${str(circle, 'name').toUpperCase()} · OUT OF CHARACTER · YOUR CIRCLE` : FeedLabels[s.channel] ?? FeedLabels.ic);
        show(this.sceneFilter, s.channel === 'ic');
        setClass(this.sceneFilter, 'active', s.mySceneOnly);
        setText(this.mode, s.chat ? 'WRITING  /  YOUR DRAFT IS PRIVATE' : 'NAVIGATION  /  ENTER TO WRITE');
        setClass(this.mode, 'sage', s.chat);
        setClass(this.mode, 'muted', !s.chat);
        setText(this.volume, s.volume.toUpperCase());
        setText(this.hint, s.channel === 'ic' ? 'SHIFT + ENTER newline · ESC keep draft · /pose /me /sit /lay /stand'
            : s.channel === 'party' ? 'SHIFT + ENTER newline · ESC keep draft · heard by your party in earshot, and by anyone close'
            : s.channel === 'partyooc' ? 'SHIFT + ENTER newline · ESC keep draft · out of character · your party, anywhere'
            : s.channel === 'chapter' ? 'SHIFT + ENTER newline · ESC keep draft · heard by your Chapter in earshot, and by anyone close'
            : s.channel === 'chapterooc' ? 'SHIFT + ENTER newline · ESC keep draft · out of character · your Chapter, anywhere'
            : s.channel === 'private' ? 'SHIFT + ENTER newline · ESC keep draft · out of character · to one friend, anywhere; kept 14 days if they\'re away'
            : circle ? 'SHIFT + ENTER newline · ESC keep draft · out of character · every member in the world'
            : 'SHIFT + ENTER newline · ESC keep draft · visible to this cell only');
        show(this.recover, !!s.failedDraft);
        setClass(this.textarea, 'writing', s.chat);
        this.textarea.readOnly = !s.chat;
        this.updateTargets();
        this.updateScene();
        this.updateFeed();
    }

    /** The scene this wolf is in, and the one just ended: its pay, stars to give, a Story to begin (doc 32, 1.1–1.2). */
    private updateScene() {
        const s = this.s, social = obj(obj(s.snapshot, 'self'), 'social');
        const ended = obj(social, 'ended');
        const sceneList = arr(social, 'scenes').filter(isObject);
        const scenes = sceneList.length ? sceneList : [obj(social, 'scene')].filter(isObject);
        const stories = arr(social, 'stories').filter(isObject);
        this.warnQuiet(scenes);
        // The quiet countdown in whole minutes, so the line is rebuilt once a minute, not on every snapshot.
        const shown = scenes.map(sc => ({...sc, endsIn: Math.ceil(num(sc, 'endsIn') / 60)}));
        // Stars this wolf gave that it may still tag (doc 51): their ids only in the key, so the countdown doesn't rebuild it.
        const given = arr(social, 'starsGiven').filter(isObject);
        const nearby = arr(social, 'nearby').filter(isObject);
        // Howls one may still join (doc 51): their direction and distance, never who.
        const calls = [...s.howls.entries()].filter(([, h]) => h.canJoin && h.until > s.clock);
        const key = JSON.stringify([shown, ended, stories.map(st => [str(st, 'id'), str(st, 'state'), bool(st, 'mine')]),
            given.map(g => str(g, 'id')), nearby, calls.map(([id, h]) => [id, h.wolves]), this.adding, s.shelf.open, [...this.openLogs].map(id => [id, s.fightLogs.get(id)?.length ?? -1])]);
        if (key === this.sceneKey) return;
        this.sceneKey = key;
        this.sceneBar.replaceChildren();
        for (const g of given) {
            // "What was it for?" One more click tags the star, for good (optional).
            const row = el('div', 'profile-veils star-tags', this.sceneBar);
            el('span', 'gold small', row, `★ Your star to ${str(g, 'to')}: what was it for?`);
            for (const t of arr(g, 'tags').filter(isObject)) {
                const chip = button(str(t, 'name').toUpperCase(), 'small chip', row,
                    () => this.s.send({type: 'social', verb: 'startag', star: str(g, 'id'), tag: str(t, 'id')}));
                chip.dataset.tag = str(t, 'id');
            }
        }
        for (const scene of scenes) {
            // Each scene one is in (a party's beside the room's): who with, what pay still needs, and, with two, where
            // one's next words count (doc 08).
            const row = el('div', 'scene-row', this.sceneBar);
            const with_ = arr(scene, 'with').filter((w): w is string => typeof w === 'string');
            const head = el('div', 'story-row', row);
            el('span', 'label sage', head, bool(scene, 'fight') ? 'FIGHT SCENE' : bool(scene, 'party') ? 'PARTY SCENE' : 'IN A SCENE');
            el('span', '', head, ` with ${with_.join(', ') || 'no one yet'} · ${num(scene, 'turns')} ${num(scene, 'turns') === 1 ? 'turn' : 'turns'}`);
            if (scenes.length > 1 && bool(scene, 'next')) el('span', 'label gold', head, '← YOUR WORDS GO HERE').title =
                'Said now, your words count toward this scene' + (bool(scene, 'party') ? ' (a party mate is here)' : '');
            if (!bool(scene, 'fight')) {
                const leave = button('LEAVE', 'small', head, () => {
                    if (window.confirm('Step out of this scene? You are paid now if you have said enough; the others carry on.'))
                        s.sendSocial({verb: 'leave', session: str(scene, 'id')});
                });
                leave.title = 'Step out of this scene: settled for you now, the others carry on';
            }
            el('span', 'muted small', row, sceneNeedsLabel(scene));
            const quiet = sceneQuietLabel(scene);
            if (quiet) el('span', 'scene-quiet small', row, quiet);
            if (!bool(scene, 'fight') && str(scene, 'openness')) {
                // Who may come in (doc 51): any of its wolves may change it, the one lit is how it stands.
                const door = el('div', 'profile-veils scene-door', row);
                for (const [value, label, tip] of [['open', 'OPEN', 'Anyone near may join in'], ['knock', 'KNOCK', 'Others knock, and one of you lets them in'],
                    ['private', 'PRIVATE', 'Only its wolves']] as const) {
                    const b = button(label, str(scene, 'openness') === value ? 'small active' : 'small', door,
                        () => s.sendSocial({verb: 'openness', session: str(scene, 'id'), value}));
                    b.title = tip;
                    b.dataset.openness = value;
                }
                for (const k of arr(scene, 'knocks').filter(isObject)) {
                    const knock = el('div', 'profile-veils scene-knock', row);
                    el('span', 'gold small', knock, `${str(k, 'name')} is knocking`);
                    button('LET IN', 'small', knock, () => s.sendSocial({verb: 'admit', session: str(scene, 'id'), who: str(k, 'id')}));
                    button('NOT NOW', 'small', knock, () => s.sendSocial({verb: 'refuse', session: str(scene, 'id'), who: str(k, 'id')}));
                }
            }
        }
        for (const [, h] of calls) {
            const row = el('div', 'scene-row howl-call', this.sceneBar);
            const toward = ['NORTH', 'NORTH-EAST', 'EAST', 'SOUTH-EAST', 'SOUTH', 'SOUTH-WEST', 'WEST', 'NORTH-WEST'][Math.floor(((h.bearing + 22.5) % 360) / 45)];
            el('span', 'label gold', row, `A HOWL TO THE ${toward} · ${h.band.toUpperCase()}${h.wolves > 1 ? ` · ${h.wolves} WOLVES` : ''}`);
            if (h.status) el('span', 'muted small', row, ` ${h.status}`);
            button('JOIN THE HOWL', 'small', row, () => s.activate({rect: noRect, action: 'howl', target: ''})).title = 'Howl with them: a chorus carries further';
        }
        for (const n of nearby) {
            // A scene here one could join, or knock on (doc 51): how many wolves, never who.
            const row = el('div', 'scene-row scene-nearby', this.sceneBar);
            const open = str(n, 'openness') === 'open';
            // Who is in it, on hover (the user, 2026-10-07): each wolf's portrait and name as this wolf knows them,
            // scrolling when there are many.
            const who = el('div', 'scene-who', row);
            el('div', 'label muted', who, 'IN THIS SCENE');
            for (const w of arr(n, 'who').filter(isObject)) {
                const line = el('div', 'scene-who-wolf', who);
                const face = el('canvas', 'scene-who-face', line);
                face.width = face.height = 40;
                const c = face.getContext('2d');
                const stage = str(w, 'lifeStage');
                if (c && this.portraits)
                    drawPortrait(c, this.portraits, obj(w, 'appearance'), stage === 'young' ? 6 : stage === 'adolescent' ? 13 : stage === 'old' ? 65 : 18,
                        0, 0, 40, 40, str(w, 'artwork') || undefined);
                el('span', '', line, str(w, 'name'));
            }
            el('span', 'label muted', row, open ? 'A SCENE HERE · OPEN' : 'A SCENE HERE · KNOCK TO JOIN');
            el('span', 'small', row, ` ${num(n, 'wolves')} ${num(n, 'wolves') === 1 ? 'wolf' : 'wolves'}`);
            if (open) button('JOIN', 'small', row, () => s.sendSocial({verb: 'join', session: str(n, 'id')})).title =
                'Join this scene: your next words count in it';
            else if (bool(n, 'knocked')) el('span', 'muted small', row, ' · knocked');
            else button('KNOCK', 'small', row, () => s.sendSocial({verb: 'knock', session: str(n, 'id')})).title =
                'Ask to join: one of its wolves may let you in';
        }
        if (ended) {
            const row = el('div', 'scene-ended', this.sceneBar);
            const fight = bool(ended, 'fight');
            const session = str(ended, 'id');
            const targets = arr(ended, 'starTargets').filter(isObject), starred = arr(ended, 'starred').filter(isObject);
            if (fight) {
                // The fight's roleplay review (doc 33): a Gold Star to each who played it well, one each, as many as like.
                el('span', 'label gold', row, `ROLEPLAY REVIEW · FIGHT OVER · +${num(ended, 'xp')} SOCIAL`);
                el('span', 'muted small', row, bool(ended, 'talked') ? 'Paid for the fight, and for roleplaying it, as a scene pays.'
                    : 'Paid for the fight. Talk it through next time: roleplay in a fight pays as a scene does, and stars are how it is thanked.');
                if (targets.length || starred.length) el('span', 'muted small', row, 'Who roleplayed it well? Give each a Gold Star:');
            } else el('span', 'label gold', row, `THE SCENE${str(ended, 'place') ? ` AT ${str(ended, 'place').toUpperCase()}` : ''}` +
                `${num(ended, 'minutes') ? ` · ${Math.trunc(num(ended, 'minutes'))} MIN` : ''} · +${num(ended, 'xp')} SOCIAL`);
            // The scene's card (doc 51, Phase 5): who was in it, and what happened, never word or turn counts.
            const with_ = arr(ended, 'with').filter((w): w is string => typeof w === 'string');
            if (with_.length) el('span', 'muted small', row, `With ${with_.join(', ')}.`);
            if (bool(ended, 'goingOn')) el('span', 'muted small', row, 'You stepped out; the others carry on. Stars when it ends.');
            const tally = obj(ended, 'tally');
            if (tally) {
                const parts = [['landed', 'landed'], ['raised', 'raised'], ['covered', 'covered'], ['guarded', 'on guard'], ['dealt', 'dealt'], ['taken', 'taken']]
                    .filter(([k]) => num(tally, k) > 0).map(([k, w]) => `${Math.trunc(num(tally, k))} ${w}`);
                if (parts.length) el('span', 'small', row, parts.join(' · '));
            }
            const moments = el('div', 'scene-moments', row);
            for (const m of arr(ended, 'moments').filter(isObject)) {
                const line = el('div', 'small', moments, str(m, 'text'));
                if (str(m, 'fight')) this.fightLog(line, str(m, 'fight'));
            }
            if (num(ended, 'logLines') > 0) this.fightLog(moments, session);
            for (const t of targets)
                button(`★ ${str(t, 'name')}`, 'small', row, () => s.sendSocial({verb: 'star', session, target: str(t, 'id')})).title =
                    `Give ${str(t, 'name')} a Gold Star for this ${fight ? 'fight' : 'scene'}`;
            for (const t of starred) {
                const given = button(`★ ${str(t, 'name')} ✓`, 'small given', row, () => undefined);
                given.disabled = true;
                given.title = `You gave ${str(t, 'name')} a Gold Star`;
            }
            // Into a Story (doc 51, Phase 7: books are the one kind of Story): one of one's open books, or a new one.
            if (this.adding !== session)
                button('ADD TO A STORY', 'small', row, () => {
                    this.adding = session;
                    s.sendBook('shelf', {tab: 'shelf', filter: 'all', offset: 0});
                    this.sceneKey = '';
                }).title = 'Put this scene in one of your Story books, or begin one with it';
            else {
                const pick = el('div', 'profile-veils add-to-story', row);
                for (const b of s.shelf.open)
                    button(`INTO "${str(b, 'title')}"`, 'small', pick, () => { s.sendBook('link', {book: str(b, 'id'), session}); this.adding = ''; });
                const title = el('input', 'profile-input', pick);
                title.placeholder = 'Or a new book\'s title';
                title.maxLength = 80;
                button('START A BOOK', 'small', pick, () => {
                    if (title.value.trim()) s.sendBook('start', {title: title.value.trim(), session});
                    this.adding = '';
                });
            }
        }
        show(this.sceneBar, scenes.length > 0 || !!ended || nearby.length > 0 || given.length > 0 || calls.length > 0);
    }

    private warned = new Set<string>();
    /** A quiet scene with five minutes left says so once (doc 08); nothing else interrupts. */
    private warnQuiet(scenes: readonly Record<string, unknown>[]) {
        for (const scene of scenes) {
            const id = str(scene, 'id');
            if (bool(scene, 'quiet') && !bool(scene, 'fight') && num(scene, 'endsIn') <= 300) {
                if (!this.warned.has(id)) {
                    this.warned.add(id);
                    this.s.showToast(`Your scene${scenes.length > 1 && bool(scene, 'party') ? ' with your party' : ''} has gone quiet: it ends in ${Math.max(1, Math.ceil(num(scene, 'endsIn') / 60))} min unless someone speaks.`);
                }
            } else if (!bool(scene, 'quiet')) this.warned.delete(id);
        }
    }

    /** A tab for each circle, with its unread count; rebuilt only when they change. */
    private updateCircleTabs() {
        const s = this.s;
        const key = JSON.stringify([s.channel, s.circles.map(c => [str(c, 'id'), str(c, 'name')]), s.unreadCircles]);
        if (key === this.circleKey) return;
        this.circleKey = key;
        this.circleTabs.replaceChildren();
        for (const c of s.circles) {
            const id = str(c, 'id'), unread = s.unreadCircles[id] ?? 0;
            const tab = button(`${str(c, 'name').toUpperCase()}${unread ? ` · ${unread}` : ''}`, 'tab', this.circleTabs,
                () => s.activate({rect: noRect, action: `circle:${id}`, target: ''}));
            setClass(tab, 'active', s.channel === `circle:${id}`);
            setClass(tab, 'unread', unread > 0);
            tab.title = `${str(c, 'name')}: your circle, out of character`;
            tab.dataset.circle = id;
        }
    }

    /** A fight's log, blow by blow (doc 51): asked for when first opened, then shown under its moment. */
    private fightLog(parent: HTMLElement, session: string) {
        const s = this.s;
        const lines = s.fightLogs.get(session);
        const open = this.openLogs.has(session);
        button(open ? 'HIDE THE FIGHT' : 'THE FIGHT, BLOW BY BLOW', 'small fight-log-toggle', parent, () => {
            if (open) this.openLogs.delete(session);
            else {
                this.openLogs.add(session);
                if (!lines) s.sendSocial({verb: 'fightlog', session});
            }
            this.sceneKey = '';
        });
        if (open && lines) {
            const box = el('div', 'fight-log', parent);
            for (const l of lines) el('div', 'fight-line', box, l);
        }
    }
    private openLogs = new Set<string>();
    private adding = '';                    // The ended scene being put into a Story (doc 51).

    private targetsKey = '';
    /** Above the composer: whom the next words go to, each chosen wolf a chip with ×. */
    private updateTargets() {
        const s = this.s;
        const {targets, nearby} = s.speakingTo();
        const key = s.channel === 'private' ? JSON.stringify([s.privateTo, s.friends.map(f => [str(f, 'handle'), bool(f, 'online')])])
            : s.channel !== 'ic' ? s.channel : JSON.stringify([targets.map(t => [t.id, t.name]), nearby?.name ?? '']);
        if (key === this.targetsKey) return;
        this.targetsKey = key;
        this.targets.replaceChildren();
        if (s.channel === 'party') {
            el('span', 'label muted', this.targets, 'SPEAKING TO');
            el('span', 'party', this.targets, 'your party');
            el('span', 'muted small hint', this.targets, '· only those close enough to hear');
        }
        if (s.channel === 'private') {
            // Which friend the next private message goes to: one chip each, here or away.
            el('span', 'label muted', this.targets, 'TO');
            if (!s.friends.length) el('span', 'muted small hint', this.targets, 'no friends yet · add them in FRIENDS');
            for (const f of s.friends) {
                const handle = str(f, 'handle');
                const chip = button(`${bool(f, 'online') ? '●' : '○'} ${handle}`, handle === s.privateTo ? 'chip pick active' : 'chip pick', this.targets,
                    () => s.activate({rect: noRect, action: 'private_to', target: handle}));
                chip.title = bool(f, 'online') ? `${handle} is here` : `${handle} is away: it will reach them when they're next here`;
            }
            return;
        }
        if (s.channel !== 'ic') return;
        el('span', 'label muted', this.targets, targets.length ? 'TALKING TO' : 'SPEAKING TO');
        if (!targets.length) {
            el('span', nearby ? 'sage' : 'muted', this.targets, nearby ? `${nearby.name} (nearby)` : 'no one in particular');
            el('span', 'muted small hint', this.targets, '· click a name in In Sight to choose');
            return;
        }
        for (const t of targets) {
            const chip = el('span', 'chip', this.targets, t.name);
            button('×', 'chip-x', chip, () => s.toggleTarget(t.id)).title = `Stop speaking to ${t.name}`;
        }
        el('span', 'muted small hint', this.targets, '· Esc on the map lets go');
    }

    private updateFeed() {
        const s = this.s;
        const filtered = s.channel === 'ic' && s.mySceneOnly;
        if (this.channel !== s.channel || this.filtered !== filtered) {
            this.channel = s.channel;
            this.filtered = filtered;
            for (const {row} of this.shown.values()) row.remove();
            this.shown.clear();
            this.stick = true;
        }
        const keep = new Set<Post>();
        let waiting = 0, added = false;
        const posts = s.posts.slice(-MaxShown);
        for (const post of posts) {
            if (!post.system && (s.channel === 'party' ? !post.party : s.channel === 'chapter' ? !post.chapter : post.channel !== s.channel)) continue;
            if (post.encounter && s.battle && post.encounter.id === s.battle.id) continue;   // Told beside it, line by line (combat.ts).
            // My scene only: its lines, one's own, the world's, and anything meant for one ("→ you").
            if (filtered && !keptByMyScene(post, str(obj(s.snapshot, 'self'), 'name'))) continue;
            if (post.revealed === 0 && !post.system) {
                ++waiting;
                continue;
            }
            keep.add(post);
            let shown = this.shown.get(post);
            if (!shown) {
                const row = el('div', post.system ? 'post system' : post.faint ? 'post faint' : post.party ? 'post party' : 'post');
                const color = css(post.system ? Muted : speakingColor(post.color));
                row.style.setProperty('--voice', color);
                // Its scene (doc 51, §7): one's own scene's lines carry its colour as a bar; an open or knock scene
                // nearby, its bar and a tag. Everything else, Private scenes' talk included, reads normally, never faded.
                if (post.scene) {
                    row.classList.add(post.scene.mine ? 'scene-mine' : 'scene-other');
                    row.style.setProperty('--scene', post.scene.colour);
                }
                const speaker = el('div', 'speaker', row, post.speaker.toUpperCase());
                // Whom it was for: "→ you" stands out, so a reply meant for the player is never lost in a crowd.
                if (post.to.length) el('span', post.to.includes('you') ? 'to you' : 'to', speaker, `  →  ${post.to.join(', ')}`);
                if (post.muffled) el('span', 'muffled', speaker, '(muffled)');
                if (post.scene && !post.scene.mine && post.scene.openness)
                    el('span', 'scene-tag', speaker, `scene${post.scene.place ? ` at ${post.scene.place}` : ''} · ${post.scene.openness === 'open' ? 'open' : 'knock to join'}`);
                // A private message kept while one was away (doc 50): when it was sent.
                if (post.kept) el('span', 'muffled', speaker, post.outgoing ? '(kept until they are here)' : `(while you were away · ${sentWhen(post.sentAt ?? 0)})`);
                // Another's line: mute, block or report its author (doc 50), by the line's number, never their id.
                const ownLine = post.speaker === str(obj(this.s.snapshot, 'self'), 'name') || !!post.outgoing;
                if (post.sequence !== undefined && !ownLine && !post.system) {
                    const flag = button('⚑', 'act line-flag', speaker, () => this.s.openSafety({line: post.sequence, label: post.speaker}));
                    flag.title = 'Mute, block or report whoever said this';
                }
                const text = el('div', 'words', row);
                this.feed.append(row);
                this.shown.set(post, shown = {row, text, revealed: -1});
                if (post.encounter) {
                    // One entry per fight (doc 18): the latest, and the whole of it on Expand.
                    const enc = post.encounter;
                    row.classList.add('encounter');
                    const lines = el('div', 'encounter-lines', row);
                    show(lines, false);
                    const toggle = button('Expand', 'act', speaker, () => {
                        enc.expanded = !enc.expanded;
                        toggle.textContent = enc.expanded ? 'Collapse' : 'Expand';
                        show(lines, enc.expanded);
                        if (enc.expanded) lines.scrollTop = lines.scrollHeight;
                    });
                    shown.lines = lines;
                }
                added = true;
            }
            if (post.encounter && shown.lines && shown.version !== post.encounter.version) {
                const lines = shown.lines, atEnd = lines.scrollTop + lines.clientHeight >= lines.scrollHeight - 4;
                shown.version = post.encounter.version;
                lines.replaceChildren(...post.encounter.lines.map(l => el('div', `fight-line ${l.kind}`, undefined, l.text)));
                if (atEnd) lines.scrollTop = lines.scrollHeight;     // Following the newest, unless reading back.
                shown.revealed = -1;
            }
            if (shown.revealed !== post.revealed) {
                shown.revealed = post.revealed;
                shown.text.textContent = post.text.slice(0, post.revealed);
                added = true;
            }
        }
        for (const [post, {row}] of this.shown)
            if (!keep.has(post)) {
                row.remove();
                this.shown.delete(post);
            }
        show(this.empty, keep.size === 0);
        setText(this.queued, waiting > 0 ? `${waiting} QUEUED` : '');
        if (added && this.stick) this.feed.scrollTop = this.feed.scrollHeight;
    }
}

const FeedLabels: Record<string, string> = {
    ic: 'NEARBY VOICES & ACTIONS',
    party: 'SAID TO YOUR PARTY · IN WORLD',
    partyooc: 'OUT OF CHARACTER · YOUR PARTY',
    chapter: 'SAID TO YOUR CHAPTER · IN WORLD',
    chapterooc: 'OUT OF CHARACTER · YOUR CHAPTER',
    ooc: 'OUT OF CHARACTER · THIS CELL',
    private: 'PRIVATE MESSAGES · OUT OF CHARACTER · YOUR FRIENDS',
};

/** When a kept private message was sent: "today 14:05", or the date. */
function sentWhen(at: number): string {
    if (!at) return 'earlier';
    const d = new Date(at * 1000), now = new Date();
    const time = d.toLocaleTimeString([], {hour: '2-digit', minute: '2-digit'});
    return d.toDateString() === now.toDateString() ? `today ${time}` : `${d.toLocaleDateString([], {day: 'numeric', month: 'short'})} ${time}`;
}

export const noRect = {left: 0, top: 0, right: 0, bottom: 0};
