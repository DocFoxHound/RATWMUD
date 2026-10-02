// Small helpers for the HTML panels around the map (Docs/Design/29-client-polish.md, phase 2). The panels are plain
// elements updated in place each frame; these keep that cheap (nothing is written that hasn't changed) and keep the
// keyboard on the map (buttons never take focus).

export function el<K extends keyof HTMLElementTagNameMap>(tag: K, className = '', parent?: HTMLElement, text?: string): HTMLElementTagNameMap[K] {
    const e = document.createElement(tag);
    if (className) e.className = className;
    if (text !== undefined) e.textContent = text;
    parent?.append(e);
    return e;
}

/** A button that never takes keyboard focus (WASD stays with the map) and acts on press. */
export function button(label: string, className: string, parent: HTMLElement | undefined, act: (e: MouseEvent) => void): HTMLButtonElement {
    const b = el('button', className, parent, label);
    b.type = 'button';
    b.tabIndex = -1;
    b.addEventListener('mousedown', e => e.preventDefault());
    b.addEventListener('click', e => act(e));
    return b;
}

const texts = new WeakMap<Node, string>();
/** Sets an element's text only when it changed. */
export function setText(e: HTMLElement, text: string) {
    if (texts.get(e) === text) return;
    texts.set(e, text);
    e.textContent = text;
}

/** Toggles a class only when it changed. */
export function setClass(e: HTMLElement, name: string, on: boolean) {
    if (e.classList.contains(name) !== on) e.classList.toggle(name, on);
}

const styles = new WeakMap<HTMLElement, Map<string, string>>();
/** Sets one style property only when it changed. */
export function setStyle(e: HTMLElement, property: string, value: string) {
    let m = styles.get(e);
    if (!m) styles.set(e, m = new Map());
    if (m.get(property) === value) return;
    m.set(property, value);
    e.style.setProperty(property, value);
}

export function show(e: HTMLElement, visible: boolean) {
    setStyle(e, 'display', visible ? '' : 'none');
}
