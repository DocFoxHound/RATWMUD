// The story column: the place, the transcript and the composer. The main focus of the screen, as it always was; now
// real text, so wrapping, selecting and scrolling are the browser's.
import {css} from '../color.ts';
import {Muted, speakingColor} from '../theme.ts';
import type {GameState, Post} from '../../game/state.ts';
import {inParty, withPlayers} from '../../game/party.ts';
import {button, el, setClass, setText, show} from './dom.ts';
import {arr, bool, isObject, num, obj, str} from '../../game/json.ts';

const MaxShown = 300;

export class StoryPanel {
    readonly root: HTMLElement;
    readonly textarea: HTMLTextAreaElement;
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
    private mode: HTMLElement;
    private queued: HTMLElement;
    private volume: HTMLButtonElement;
    private hint: HTMLElement;
    private recover: HTMLButtonElement;
    private shown = new Map<Post, {row: HTMLElement; text: HTMLElement; revealed: number; version?: number; lines?: HTMLElement}>();
    private channel = '';
    private stick = true;                   // Following the newest post (until the reader scrolls up).

    constructor(parent: HTMLElement, state: GameState) {
        this.s = state;
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
        this.place = el('h2', 'place', this.root);
        this.scene = el('p', 'scene', this.root);
        this.sceneBar = el('div', 'scene-bar', this.root);
        this.feedLabel = el('div', 'label muted feed-label', this.root);
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
        setText(this.feedLabel, FeedLabels[s.channel] ?? FeedLabels.ic);
        setText(this.mode, s.chat ? 'WRITING  /  YOUR DRAFT IS PRIVATE' : 'NAVIGATION  /  ENTER TO WRITE');
        setClass(this.mode, 'sage', s.chat);
        setClass(this.mode, 'muted', !s.chat);
        setText(this.volume, s.volume.toUpperCase());
        setText(this.hint, s.channel === 'ic' ? 'SHIFT + ENTER newline · ESC keep draft · /pose /me /sit /lay /stand'
            : s.channel === 'party' ? 'SHIFT + ENTER newline · ESC keep draft · heard by your party in earshot, and by anyone close'
            : s.channel === 'partyooc' ? 'SHIFT + ENTER newline · ESC keep draft · out of character · your party, anywhere'
            : s.channel === 'chapter' ? 'SHIFT + ENTER newline · ESC keep draft · heard by your Chapter in earshot, and by anyone close'
            : s.channel === 'chapterooc' ? 'SHIFT + ENTER newline · ESC keep draft · out of character · your Chapter, anywhere'
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
        const scene = obj(social, 'scene'), ended = obj(social, 'ended');
        const stories = arr(social, 'stories').filter(isObject);
        const key = JSON.stringify([scene, ended, stories.map(st => [str(st, 'id'), str(st, 'state'), bool(st, 'mine')])]);
        if (key === this.sceneKey) return;
        this.sceneKey = key;
        this.sceneBar.replaceChildren();
        if (scene) {
            const with_ = arr(scene, 'with').filter((w): w is string => typeof w === 'string');
            el('span', 'label sage', this.sceneBar, bool(scene, 'party') ? 'PARTY SCENE' : 'IN A SCENE');
            el('span', '', this.sceneBar, ` with ${with_.join(', ') || 'others'} · ${num(scene, 'turns')} turns` +
                (bool(scene, 'quiet') ? ' · quiet' : ''));
        }
        if (ended) {
            const row = el('div', 'scene-ended', this.sceneBar);
            const fight = bool(ended, 'fight');
            const session = str(ended, 'id');
            const targets = arr(ended, 'starTargets').filter(isObject), starred = arr(ended, 'starred').filter(isObject);
            if (fight) {
                // The fight's roleplay review (doc 33): a Gold Star to each who played it well, one each, as many as like.
                el('span', 'label gold', row, `ROLEPLAY REVIEW · FIGHT OVER · +${num(ended, 'xp')} SOCIAL`);
                el('span', 'muted small', row, bool(ended, 'talked') ? 'Paid for the fight, and twice for roleplaying it.'
                    : 'Paid for the fight. Talk it through next time: roleplay in a fight pays twice.');
                if (targets.length || starred.length) el('span', 'muted small', row, 'Who roleplayed it well? Give each a Gold Star:');
            } else el('span', 'label gold', row, `SCENE ENDED · +${num(ended, 'xp')} SOCIAL`);
            for (const t of targets)
                button(`★ ${str(t, 'name')}`, 'small', row, () => s.sendSocial({verb: 'star', session, target: str(t, 'id')})).title =
                    `Give ${str(t, 'name')} a Gold Star for this ${fight ? 'fight' : 'scene'}`;
            for (const t of starred) {
                const given = button(`★ ${str(t, 'name')} ✓`, 'small given', row, () => undefined);
                given.disabled = true;
                given.title = `You gave ${str(t, 'name')} a Gold Star`;
            }
            if (bool(ended, 'storyable')) {
                const mine = stories.find(st => bool(st, 'mine') && str(st, 'state') === 'active');
                if (mine)
                    button(`ADD TO "${str(mine, 'name')}"`, 'small', row, () => s.sendSocial({verb: 'extend', story: str(mine, 'id'), session}));
                button('MAKE IT A STORY', 'small', row, () => {
                    const name = window.prompt('A name for the Story');
                    if (name?.trim()) s.sendSocial({verb: 'propose', session, name: name.trim()});
                });
            }
        }
        show(this.sceneBar, !!scene || !!ended);
    }

    private targetsKey = '';
    /** Above the composer: whom the next words go to, each chosen wolf a chip with ×. */
    private updateTargets() {
        const s = this.s;
        const {targets, nearby} = s.speakingTo();
        const key = s.channel !== 'ic' ? s.channel : JSON.stringify([targets.map(t => [t.id, t.name]), nearby?.name ?? '']);
        if (key === this.targetsKey) return;
        this.targetsKey = key;
        this.targets.replaceChildren();
        if (s.channel === 'party') {
            el('span', 'label muted', this.targets, 'SPEAKING TO');
            el('span', 'party', this.targets, 'your party');
            el('span', 'muted small hint', this.targets, '· only those close enough to hear');
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
        if (this.channel !== s.channel) {
            this.channel = s.channel;
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
                const speaker = el('div', 'speaker', row, post.speaker.toUpperCase());
                // Whom it was for: "→ you" stands out, so a reply meant for the player is never lost in a crowd.
                if (post.to.length) el('span', post.to.includes('you') ? 'to you' : 'to', speaker, `  →  ${post.to.join(', ')}`);
                if (post.muffled) el('span', 'muffled', speaker, '(muffled)');
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
};

export const noRect = {left: 0, top: 0, right: 0, bottom: 0};
