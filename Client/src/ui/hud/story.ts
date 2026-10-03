// The story column: the place, the transcript and the composer. The main focus of the screen, as it always was; now
// real text, so wrapping, selecting and scrolling are the browser's.
import {css} from '../color.ts';
import {Muted, speakingColor} from '../theme.ts';
import type {GameState, Post} from '../../game/state.ts';
import {inParty} from '../../game/party.ts';
import {button, el, setClass, setText, show} from './dom.ts';

const MaxShown = 300;

export class StoryPanel {
    readonly root: HTMLElement;
    readonly textarea: HTMLTextAreaElement;
    readonly targets: HTMLElement;          // Talk targets (phase 4) sit above the composer.
    private s: GameState;
    private place: HTMLElement;
    private scene: HTMLElement;
    private feedLabel: HTMLElement;
    private feed: HTMLElement;
    private empty: HTMLElement;
    private ic: HTMLButtonElement;
    private ooc: HTMLButtonElement;
    private party: HTMLButtonElement;
    private partyOoc: HTMLButtonElement;
    private mode: HTMLElement;
    private queued: HTMLElement;
    private volume: HTMLButtonElement;
    private hint: HTMLElement;
    private recover: HTMLButtonElement;
    private shown = new Map<Post, {row: HTMLElement; text: HTMLElement; revealed: number}>();
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
        this.ooc = button('LOCAL OOC', 'tab', tabs, () => state.activate({rect: noRect, action: 'ooc', target: ''}));
        this.place = el('h2', 'place', this.root);
        this.scene = el('p', 'scene', this.root);
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
        show(this.partyOoc, grouped);
        setText(this.feedLabel, FeedLabels[s.channel] ?? FeedLabels.ic);
        setText(this.mode, s.chat ? 'WRITING  /  YOUR DRAFT IS PRIVATE' : 'NAVIGATION  /  ENTER TO WRITE');
        setClass(this.mode, 'sage', s.chat);
        setClass(this.mode, 'muted', !s.chat);
        setText(this.volume, s.volume.toUpperCase());
        setText(this.hint, s.channel === 'ic' ? 'SHIFT + ENTER newline · ESC keep draft · /pose /me /sit /lay /stand'
            : s.channel === 'party' ? 'SHIFT + ENTER newline · ESC keep draft · heard by your party in earshot, and by anyone close'
            : s.channel === 'partyooc' ? 'SHIFT + ENTER newline · ESC keep draft · out of character · your party, anywhere'
            : 'SHIFT + ENTER newline · ESC keep draft · visible to this cell only');
        show(this.recover, !!s.failedDraft);
        setClass(this.textarea, 'writing', s.chat);
        this.textarea.readOnly = !s.chat;
        this.updateTargets();
        this.updateFeed();
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
            if (!post.system && (s.channel === 'party' ? !post.party : post.channel !== s.channel)) continue;
            if (post.revealed === 0 && !post.system) {
                ++waiting;
                continue;
            }
            keep.add(post);
            let shown = this.shown.get(post);
            if (!shown) {
                const row = el('div', post.system ? 'post system' : post.party ? 'post party' : 'post');
                const color = css(post.system ? Muted : speakingColor(post.color));
                row.style.setProperty('--voice', color);
                const speaker = el('div', 'speaker', row, post.speaker.toUpperCase());
                // Whom it was for: "→ you" stands out, so a reply meant for the player is never lost in a crowd.
                if (post.to.length) el('span', post.to.includes('you') ? 'to you' : 'to', speaker, `  →  ${post.to.join(', ')}`);
                const text = el('div', 'words', row);
                this.feed.append(row);
                this.shown.set(post, shown = {row, text, revealed: -1});
                added = true;
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
    ooc: 'OUT OF CHARACTER · THIS CELL',
};

export const noRect = {left: 0, top: 0, right: 0, bottom: 0};
