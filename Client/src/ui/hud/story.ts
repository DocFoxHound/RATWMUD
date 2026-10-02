// The story column: the place, the transcript and the composer. The main focus of the screen, as it always was; now
// real text, so wrapping, selecting and scrolling are the browser's.
import {css} from '../color.ts';
import {Muted, speakingColor} from '../theme.ts';
import type {GameState, Post} from '../../game/state.ts';
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
        setText(this.feedLabel, s.channel === 'ic' ? 'NEARBY VOICES & ACTIONS' : 'OUT OF CHARACTER · THIS CELL');
        setText(this.mode, s.chat ? 'WRITING  /  YOUR DRAFT IS PRIVATE' : 'NAVIGATION  /  ENTER TO WRITE');
        setClass(this.mode, 'sage', s.chat);
        setClass(this.mode, 'muted', !s.chat);
        setText(this.volume, s.volume.toUpperCase());
        setText(this.hint, s.channel === 'ic' ? 'SHIFT + ENTER newline · ESC keep draft · /pose /me /sit /lay /stand'
            : 'SHIFT + ENTER newline · ESC keep draft · visible to this cell only');
        show(this.recover, !!s.failedDraft);
        setClass(this.textarea, 'writing', s.chat);
        this.textarea.readOnly = !s.chat;
        this.updateFeed();
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
            if (post.channel !== s.channel && !post.system) continue;
            if (post.revealed === 0 && !post.system) {
                ++waiting;
                continue;
            }
            keep.add(post);
            let shown = this.shown.get(post);
            if (!shown) {
                const row = el('div', post.system ? 'post system' : 'post');
                const color = css(post.system ? Muted : speakingColor(post.color));
                row.style.setProperty('--voice', color);
                el('div', 'speaker', row, post.speaker.toUpperCase());
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

export const noRect = {left: 0, top: 0, right: 0, bottom: 0};
