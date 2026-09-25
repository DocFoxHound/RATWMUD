import { createProject, paint, cutGrid, splitCell, mergeCells, addRoom, addLink, removeLink, getCell, cellTerrain, validate, clone, normalizeProject,
  upsertFaction, upsertChapter, removeFaction, removeChapter, setTerritory } from './model.mjs';

const $ = id => document.getElementById(id);
const canvas = $('map-canvas');
const ctx = canvas.getContext('2d');
const DRAFT_KEY = 'ratw-atlas-workshop-v1';
const palette = [
  ['.', 'Open floor / path', '#aa9b75'], [',', 'Grass (modest movement cost)', '#687d59'], ['"', 'Tall grass (modest movement cost)', '#91a369'],
  ['T', 'Low table (solid, sight passes)', '#b39975'], ['#', 'Wall / dense cover (solid, opaque)', '#b6b6a0'], ['=', 'Low counter (solid, sight passes)', '#af8e60'],
  ['~', 'Shallow water (slow, traversable)', '#75989e'], [':', 'Low rise (+0.25)', '#a6a084'], ['^', 'High rise (+0.50)', '#c2ad81'], ['+', 'Door glyph (link separately)', '#c7ad81'],
];
const glyphColor = Object.fromEntries(palette.map(([glyph, , color]) => [glyph, color]));
let project, mode = 'world', activeId = '', selection = new Set(), tool = 'select', glyph = '.', brush = 1;
let history = [], future = [], hover = null, drag = null, pendingEndpoint = 'a', draftRecovery = null;
let camera = { x: 0, y: 0, scale: 14 }, viewSize = { width: 0, height: 0 }, spaceHeld = false, dirty = false, draftSaved = false;
let lastValidation = { errors: [], warnings: [] }, renderScheduled = false;
let confirmationResolver = null;

function message(text, kind = '') {
  $('message').textContent = text;
  $('message').className = kind;
  if ($('politics-dialog').open) { $('catalog-status').textContent = text; $('catalog-status').className = kind; }
}

function askConfirmation(title, description, acceptLabel = 'Continue') {
  if (confirmationResolver) return Promise.resolve(false);
  finishDrag();
  $('confirmation-title').textContent = title;
  $('confirmation-message').textContent = description;
  $('confirmation-accept').textContent = acceptLabel;
  const dialog = $('confirmation-dialog');
  dialog.returnValue = 'cancel';
  return new Promise(resolve => { confirmationResolver = resolve; dialog.showModal(); });
}
$('confirmation-dialog').addEventListener('close', () => {
  const resolve = confirmationResolver;
  confirmationResolver = null;
  resolve?.($('confirmation-dialog').returnValue === 'accept');
});

function demoProject() {
  const p = createProject(64, 48, 'The North Reach');
  const rows = [];
  for (let y = 0; y < p.height; y++) {
    const row = [];
    for (let x = 0; x < p.width; x++) {
      let tile = ',';
      const creek = 39 + Math.sin(y / 8) * 4;
      if (Math.abs(x - creek) < 1.35) tile = '~';
      if ((x * 17 + y * 23) % 29 < 3) tile = '"';
      if (Math.abs(x - creek) < 1.35) tile = '~';
      if ((x < 16 && y < 16 || x > 48 && y > 26 || x < 13 && y > 33) && (x * 7 + y * 11) % 9 < 3) tile = '#';
      if (x > 48 && y < 15 && (x + y) % 4 < 2) tile = ':';
      if (x > 54 && y < 10 && (x + y) % 3 === 0) tile = '^';
      const road = 22 + Math.round(Math.sin(x / 13) * 3);
      if (Math.abs(y - road) <= 1 || Math.abs(x - (21 + Math.round(y / 12))) <= 1 && y >= 23) tile = '.';
      if (x >= 9 && x <= 20 && y >= 17 && y <= 21) tile = '.';
      if ((x === 9 || x === 20) && y >= 13 && y <= 17 || y === 13 && x >= 9 && x <= 20) tile = '#';
      if (x >= 10 && x <= 19 && y >= 14 && y <= 17) tile = '.';
      if (y === 32 && x >= 30 && x <= 35) tile = '=';
      row.push(tile);
    }
    rows.push(row.join(''));
  }
  p.terrain = rows;
  cutGrid(p, 32, 24);
  const names = ['Hearthwood Crossing', 'The Eastern Rise', 'Old Willow Meadow', 'Riverbend'];
  p.cells.forEach((cell, index) => { cell.name = names[index] || cell.name; cell.description = 'A quiet reach of the north, shaped by weather, water and the paths of wolves.'; });
  const room = addRoom(p, 20, 14, 'The Quiet Hearth');
  room.description = 'Warmth gathers under old rafters. A long counter and scattered tables make room for conversation.';
  room.z = 0;
  room.terrain = Array.from({ length: room.height }, (_, y) => Array.from({ length: room.width }, (_, x) => x === 0 || y === 0 || x === room.width - 1 || y === room.height - 1 ? '#' : y === 3 && x > 8 && x < 17 ? '=' : [6, 12, 16].includes(x) && [7, 10].includes(y) ? 'T' : '.').join(''));
  const west = p.cells.find(c => c.x === 0 && c.y === 0);
  addLink(p, { name: 'The Quiet Hearth doorway', kind: 'door', a: { cell: west.id, x: 14, y: 18 }, b: { cell: room.id, x: 2, y: 7 }, open: false });
  p.spawn = { cell: west.id, x: 18, y: 21 };
  return normalizeProject(p);
}

function allCells() { return [...project.cells, ...project.rooms]; }
function activeCell() { return getCell(project, activeId); }
function roomFor(id) { return project.rooms.find(room => room.id === id); }
function visibleMap() {
  const cell = mode === 'cell' ? activeCell() : null;
  if (mode === 'cell' && !cell) return null;
  return cell ? { width: cell.width, height: cell.height, terrain: cellTerrain(project, cell.id), cell } : { width: project.width, height: project.height, terrain: project.terrain, cell: null };
}
function cellAt(x, y) { return project.cells.find(c => x >= c.x && y >= c.y && x < c.x + c.width && y < c.y + c.height); }
function pointFromEvent(event) {
  const rect = canvas.getBoundingClientRect();
  return { x: (event.clientX - rect.left - camera.x) / camera.scale, y: (event.clientY - rect.top - camera.y) / camera.scale };
}
function validPoint(point) {
  const map = visibleMap();
  return map && point && point.x >= 0 && point.y >= 0 && point.x < map.width && point.y < map.height;
}
function endpointAt(point) {
  if (!validPoint(point)) return null;
  const x = Math.floor(point.x), y = Math.floor(point.y);
  if (mode === 'cell') return { cell: activeId, x, y };
  const c = cellAt(x, y);
  return c ? { cell: c.id, x: x - c.x, y: y - c.y } : null;
}
function worldPoint(endpoint) {
  const cell = getCell(project, endpoint.cell);
  if (!cell) return null;
  if (mode === 'cell') return cell.id === activeId ? { x: endpoint.x + .5, y: endpoint.y + .5 } : null;
  if (roomFor(cell.id)) return null;
  return { x: cell.x + endpoint.x + .5, y: cell.y + endpoint.y + .5 };
}
function tidySelection() {
  selection = new Set([...selection].filter(id => getCell(project, id)));
  if (!getCell(project, activeId)) activeId = [...selection][0] || '';
  if (mode === 'cell' && !activeId) mode = 'world';
}
function saveDraft() {
  draftSaved = false;
  if (draftRecovery) return;
  try { localStorage.setItem(DRAFT_KEY, JSON.stringify({ savedAt: new Date().toISOString(), project })); draftSaved = true; }
  catch { message('Your browser could not save a local draft. Save JSON to keep your work.', 'error'); }
}
function commit(before, description) {
  if (JSON.stringify(before) === JSON.stringify(project)) return false;
  history.push(before);
  if (history.length > 60) history.shift();
  future = [];
  dirty = true;
  tidySelection();
  saveDraft();
  refresh();
  if (description) message(description, 'success');
  return true;
}
function transact(description, action) {
  const before = clone(project);
  try { action(); commit(before, description); return true; }
  catch (error) { project = before; message(error.message || String(error), 'error'); refresh(); return false; }
}
function undo() {
  finishDrag();
  if (!history.length) return;
  future.push(clone(project)); project = history.pop(); dirty = true; tidySelection(); saveDraft(); refresh(); message('Undid the last editing operation.');
}
function redo() {
  finishDrag();
  if (!future.length) return;
  history.push(clone(project)); project = future.pop(); dirty = true; tidySelection(); saveDraft(); refresh(); message('Restored the next editing operation.');
}
function setProject(value) {
  project = normalizeProject(value); history = []; future = []; selection = new Set(project.cells[0] ? [project.cells[0].id] : []); activeId = project.cells[0]?.id || ''; mode = 'world'; dirty = false; $('link-form').hidden = true; refresh(); fit();
}

function updateOptions(select, includeEmpty = false) {
  const previous = select.value;
  select.replaceChildren();
  if (includeEmpty) select.add(new Option('Choose a cell…', ''));
  for (const cell of allCells()) select.add(new Option(`${roomFor(cell.id) ? '↳ ' : ''}${cell.name} · ${cell.width}×${cell.height}`, cell.id));
  if ([...select.options].some(option => option.value === previous)) select.value = previous;
}
function refresh() {
  $('project-name').value = project.name;
  $('save-state').textContent = dirty ? draftRecovery ? 'New edits not autosaved · Restore or dismiss the earlier draft' : draftSaved ? 'Draft kept in this browser · Save JSON for a portable copy' : 'Local autosave unavailable · Save JSON to keep your work' : 'Local authoring · not a live world';
  $('world-mode').classList.toggle('active', mode === 'world');
  $('cell-mode').classList.toggle('active', mode === 'cell');
  $('world-count').textContent = `${project.cells.length} cells · ${project.rooms.length} interior${project.rooms.length === 1 ? '' : 's'}`;
  $('room-count').textContent = project.rooms.length;
  $('undo').disabled = !history.length; $('redo').disabled = !future.length;
  $('selection-count').textContent = `${selection.size} SELECTED`;
  $('view-title').textContent = mode === 'world' ? project.name : activeCell()?.name || 'Cell detail';
  $('view-eyebrow').textContent = mode === 'world' ? 'WORLD / CONTINUOUS AUTHORING SURFACE' : `${roomFor(activeId) ? 'DETACHED INTERIOR' : 'WORLD CELL'} / FINE DETAIL`;
  $('partition-section').hidden = mode !== 'world';
  $('canvas-empty').hidden = !!visibleMap();
  $('pointer-status').textContent = `${visibleMap()?.width || 0} × ${visibleMap()?.height || 0} tiles`;
  updateOptions($('cell-select'), true);
  $('cell-select').value = selection.size === 1 ? [...selection][0] : '';
  for (const side of ['a', 'b']) updateOptions($(`link-${side}-cell`));
  const selected = selection.size === 1 ? getCell(project, [...selection][0]) : null;
  $('metadata-form').hidden = !selected;
  $('selection-empty').hidden = !!selected;
  $('selection-empty').textContent = selection.size > 1 ? `${selection.size} cells selected. Merge them when they form a filled rectangle at the same level.` : 'Select a cell on the map, or draw terrain before making your first cut.';
  $('selection-title').textContent = selected && roomFor(selected.id) ? 'Selected interior' : 'Selected cell';
  if (selected) {
    $('cell-name').value = selected.name; $('cell-description').value = selected.description || '';
    $('cell-z').value = selected.z; $('cell-weather').value = selected.weather; $('cell-outdoors').checked = selected.outdoors;
    $('cell-light').value = selected.lighting.artificial;
    $('cell-daylight').value = selected.lighting.daylightAccess;
    $('cell-light-tone').value = selected.lighting.tone;
    $('cell-dimensions').textContent = `${selected.width} × ${selected.height} tiles · ${roomFor(selected.id) ? 'detached room' : `origin ${selected.x}, ${selected.y}`} · ${selected.id}`;
    $('drill-down').textContent = mode === 'cell' ? 'World overview ↗' : 'Drill down ↗';
  }
  $('merge-cells').disabled = selection.size < 2 || [...selection].some(roomFor);
  $('show-split').disabled = !selected || !!roomFor(selected.id);
  $('cell-mode').disabled = !activeCell();
  renderTerritory();
  renderCatalogs();
  renderLinks();
  runValidation();
  scheduleRender();
}
function renderTerritory() {
  const selected = [...selection].map(id => getCell(project, id)).filter(Boolean), first = selected[0]?.territory;
  $('territory-form').hidden = !first; $('territory-empty').hidden = !!first;
  $('territory-chapter').replaceChildren(new Option('No Chapter site', ''));
  for (const chapter of project.chapters) $('territory-chapter').add(new Option(chapter.name, chapter.id));
  $('territory-claims').replaceChildren();
  for (const faction of project.factions) {
    const label = document.createElement('label'); label.className = 'check-field';
    const input = document.createElement('input'); input.type = 'checkbox'; input.value = faction.id;
    input.checked = !!first?.claims.includes(faction.id);
    const swatch = document.createElement('i'); swatch.style.backgroundColor = faction.color;
    const name = document.createElement('span'); name.textContent = faction.name;
    label.append(input, swatch, name); $('territory-claims').append(label);
  }
  if (!project.factions.length) { const p = document.createElement('p'); p.className = 'hint'; p.textContent = 'No factions yet. Open Catalogs to create one.'; $('territory-claims').append(p); }
  if (first) {
    $('territory-region').value = first.region; $('territory-chapter').value = first.chapter;
    const mixed = selected.some(c => JSON.stringify(c.territory) !== JSON.stringify(first));
    $('territory-selection-note').textContent = mixed ? `${selected.length} cells have mixed territory settings. Showing the first; applying will replace all selected settings after confirmation.`
      : `${selected.length} cell${selected.length === 1 ? '' : 's'} selected. Region groups nearby settlements; claims and Chapter sites remain independent.`;
  }
  $('territory-legend').replaceChildren();
  for (const faction of project.factions) {
    const row = document.createElement('span'), swatch = document.createElement('i'), name = document.createElement('span');
    swatch.style.backgroundColor = faction.color; name.textContent = faction.name; row.append(swatch, name); $('territory-legend').append(row);
  }
  if (project.chapters.length) { const row = document.createElement('span'); row.textContent = '◇ Chapter site · stripes = overlapping claims'; $('territory-legend').append(row); }
}
function loadCatalogForm(kind) {
  const item = project[kind === 'faction' ? 'factions' : 'chapters'].find(entry => entry.id === $(`${kind}-select`).value);
  $(`${kind}-id`).value = item?.id || ''; $(`${kind}-id`).disabled = !!item;
  $(`${kind}-name`).value = item?.name || ''; $(`remove-${kind}`).disabled = !item;
  if (kind === 'faction') $('faction-color').value = item?.color || '#7799bb';
}
function renderCatalogs() {
  for (const kind of ['faction', 'chapter']) {
    const select = $(`${kind}-select`), previous = select.value;
    select.replaceChildren(new Option(`+ New ${kind === 'chapter' ? 'Chapter' : kind}`, ''));
    for (const entry of project[kind === 'faction' ? 'factions' : 'chapters']) select.add(new Option(`${entry.name} · ${entry.id}`, entry.id));
    if ([...select.options].some(option => option.value === previous)) select.value = previous;
    loadCatalogForm(kind);
  }
}
function runValidation() {
  try { lastValidation = validate(project); }
  catch (error) { lastValidation = { errors: [error.message], warnings: [] }; }
  const { errors, warnings } = lastValidation;
  $('validation-summary').textContent = errors.length ? `${errors.length} issue${errors.length === 1 ? '' : 's'} must be fixed before export` : `Ready to export${warnings.length ? ` · ${warnings.length} authoring note${warnings.length === 1 ? '' : 's'}` : ' · no blocking issues'}`;
  $('validation-summary').classList.toggle('invalid', !!errors.length);
  $('validation-list').replaceChildren();
  for (const text of [...errors.map(item => `Error: ${item}`), ...warnings].slice(0, 20)) {
    const li = document.createElement('li'); li.textContent = text; $('validation-list').append(li);
  }
  if (errors.length + warnings.length > 20) { const li = document.createElement('li'); li.textContent = 'More issues are listed by the export validator.'; $('validation-list').append(li); }
}
function renderLinks() {
  $('link-list').replaceChildren();
  const relevant = project.links.filter(link => !selection.size || [...selection].some(id => link.a.cell === id || link.b.cell === id));
  if (!relevant.length) { const text = document.createElement('p'); text.className = 'hint'; text.textContent = 'No explicit connections here. Compatible touching world edges connect automatically on export.'; $('link-list').append(text); }
  for (const link of relevant) {
    const row = document.createElement('div'); row.className = 'link-item';
    const label = document.createElement('div'), title = document.createElement('strong'), detail = document.createElement('small');
    title.textContent = link.name;
    detail.textContent = `${getCell(project, link.a.cell)?.name} ↔ ${getCell(project, link.b.cell)?.name} · ${link.kind}`;
    label.append(title, detail);
    const remove = document.createElement('button'); remove.type = 'button'; remove.textContent = '×'; remove.title = `Remove ${link.name}`; remove.setAttribute('aria-label', `Remove connection ${link.name}`);
    remove.addEventListener('click', async () => {
      if (!await askConfirmation('Remove this connection?', `Remove “${link.name}” and its reciprocal doorway? The terrain and both cells remain unchanged. Undo is available.`, 'Remove connection')) return;
      transact('Connection removed. Undo is available.', () => removeLink(project, link.id));
    });
    row.append(label, remove); $('link-list').append(row);
  }
}

function scheduleRender() {
  if (renderScheduled) return;
  renderScheduled = true;
  requestAnimationFrame(() => { renderScheduled = false; render(); });
}
function fit() {
  const map = visibleMap(); if (!map || !viewSize.width) return;
  camera.scale = Math.min(36, (viewSize.width - 74) / map.width, (viewSize.height - 100) / map.height);
  camera.scale = Math.max(1, camera.scale);
  camera.x = (viewSize.width - map.width * camera.scale) / 2;
  camera.y = (viewSize.height - map.height * camera.scale) / 2 - 9;
  scheduleRender();
}
function zoom(factor, center = { x: viewSize.width / 2, y: viewSize.height / 2 }) {
  const old = camera.scale;
  camera.scale = Math.max(1, Math.min(64, old * factor));
  camera.x = center.x - (center.x - camera.x) * camera.scale / old;
  camera.y = center.y - (center.y - camera.y) * camera.scale / old;
  scheduleRender();
}
function drawMarker(point, type, label = '') {
  if (!point) return;
  const x = camera.x + point.x * camera.scale, y = camera.y + point.y * camera.scale;
  const radius = Math.max(5, camera.scale * .55);
  ctx.fillStyle = '#142219'; ctx.strokeStyle = type === 'spawn' ? '#dbbf78' : '#cab6e4'; ctx.lineWidth = 1.5;
  ctx.beginPath();
  if (type === 'spawn') ctx.arc(x, y, radius, 0, Math.PI * 2);
  else { ctx.moveTo(x, y - radius); ctx.lineTo(x + radius, y); ctx.lineTo(x, y + radius); ctx.lineTo(x - radius, y); ctx.closePath(); }
  ctx.fill(); ctx.stroke();
  if (camera.scale > 8) { ctx.fillStyle = ctx.strokeStyle; ctx.font = `${Math.max(8, camera.scale * .65)}px ui-monospace, monospace`; ctx.fillText(type === 'spawn' ? 'W' : '+', x, y + .5); }
  if (label && camera.scale >= 12) { ctx.font = '10px system-ui'; ctx.textAlign = 'left'; ctx.fillStyle = '#d0c2df'; ctx.fillText(label, x + radius + 5, y); ctx.textAlign = 'center'; }
}
function render() {
  ctx.clearRect(0, 0, viewSize.width, viewSize.height);
  const map = visibleMap(); if (!map) return;
  const s = camera.scale, left = camera.x, top = camera.y;
  $('zoom-value').textContent = `${Math.round(s / 16 * 100)}%`;
  const x0 = Math.max(0, Math.floor(-left / s)), y0 = Math.max(0, Math.floor(-top / s));
  const x1 = Math.min(map.width, Math.ceil((viewSize.width - left) / s)), y1 = Math.min(map.height, Math.ceil((viewSize.height - top) / s));
  ctx.fillStyle = '#202f22'; ctx.fillRect(left, top, map.width * s, map.height * s);
  ctx.strokeStyle = '#4a5c41'; ctx.lineWidth = 1; ctx.strokeRect(left - .5, top - .5, map.width * s + 1, map.height * s + 1);
  ctx.textAlign = 'center'; ctx.textBaseline = 'middle'; ctx.font = `${Math.max(3, s * .82)}px ui-monospace, "DejaVu Sans Mono", Consolas, monospace`;
  for (let y = y0; y < y1; y++) for (let x = x0; x < x1; x++) {
    const tile = map.terrain[y][x], px = left + x * s, py = top + y * s;
    if (tile === '~') { ctx.fillStyle = '#253e3b'; ctx.fillRect(px, py, s, s); }
    if (tile === '#' || tile === '=' || tile === 'T') { ctx.fillStyle = tile === '#' ? '#414939' : '#393c29'; ctx.fillRect(px, py, s, s); }
    if (tile === '.') { ctx.fillStyle = '#393f2b'; ctx.fillRect(px, py, s, s); }
    if ($('show-heights').checked) {
      const room = map.cell && roomFor(map.cell.id);
      const key = room ? `${x},${y}` : `${x + (map.cell?.x || 0)},${y + (map.cell?.y || 0)}`;
      const heights = room ? room.heights : project.heights;
      const elevation = heights[key] ?? (tile === ':' ? .25 : tile === '^' ? .5 : 0);
      if (elevation) { ctx.fillStyle = elevation > 0 ? `rgba(209,168,103,${Math.min(.65, .1 + elevation * .1)})` : `rgba(106,135,184,${Math.min(.6, .1 + -elevation * .1)})`; ctx.fillRect(px, py, s, s); }
    }
    if (s >= 5) { ctx.fillStyle = glyphColor[tile] || '#8fa181'; ctx.fillText(tile, px + s / 2, py + s / 2 + .5); }
    else if (!['.', ','].includes(tile)) { ctx.fillStyle = glyphColor[tile] || '#8fa181'; ctx.fillRect(px + s * .25, py + s * .25, s * .5, s * .5); }
  }
  if (s >= 19) {
    ctx.strokeStyle = '#ffffff09'; ctx.lineWidth = 1; ctx.beginPath();
    for (let x = x0; x <= x1; x++) { ctx.moveTo(left + x * s, Math.max(0, top)); ctx.lineTo(left + x * s, Math.min(viewSize.height, top + map.height * s)); }
    for (let y = y0; y <= y1; y++) { ctx.moveTo(Math.max(0, left), top + y * s); ctx.lineTo(Math.min(viewSize.width, left + map.width * s), top + y * s); }
    ctx.stroke();
  }
  if ($('show-territory').checked) {
    for (const cell of (mode === 'world' ? project.cells : [map.cell]).filter(Boolean)) {
      const territory = cell.territory, x = left + (mode === 'world' ? cell.x : 0) * s, y = top + (mode === 'world' ? cell.y : 0) * s;
      const w = cell.width * s, h = cell.height * s, factions = territory.claims.map(id => project.factions.find(f => f.id === id));
      if (factions.length) {
        ctx.save(); ctx.beginPath(); ctx.rect(x, y, w, h); ctx.clip();
        for (let stripe = 0, offset = 0; offset < w; stripe++, offset += 20) {
          ctx.fillStyle = factions[stripe % factions.length].color; ctx.globalAlpha = .16; ctx.fillRect(x + offset, y, 20, h);
        }
        ctx.globalAlpha = .8;
        factions.forEach((faction, index) => { ctx.fillStyle = faction.color; ctx.fillRect(x + index * w / factions.length, y + h - 3, w / factions.length, 3); });
        ctx.restore();
      }
      if (territory.chapter && w > 80 && h > 65) {
        const chapter = project.chapters.find(c => c.id === territory.chapter);
        ctx.save(); ctx.beginPath(); ctx.rect(x + 5, y + h - 28, w - 10, 23); ctx.clip();
        ctx.font = '10px system-ui'; ctx.textAlign = 'left'; ctx.fillStyle = '#101a15ee'; ctx.fillRect(x + 5, y + h - 27, Math.min(w - 10, ctx.measureText(chapter.name).width + 27), 21);
        ctx.fillStyle = '#ead4a6'; ctx.fillText(`◇ ${chapter.name}`, x + 10, y + h - 16); ctx.restore();
      }
    }
  }
  if (mode === 'world' && $('show-cuts').checked) {
    for (const cell of project.cells) {
      const selected = selection.has(cell.id), x = left + cell.x * s, y = top + cell.y * s;
      ctx.strokeStyle = selected ? '#d3bb78' : '#87906c'; ctx.lineWidth = selected ? 2 : 1;
      ctx.setLineDash(selected ? [] : [5, 4]); ctx.strokeRect(x, y, cell.width * s, cell.height * s); ctx.setLineDash([]);
      if (cell.width * s > 95 && cell.height * s > 45) {
        ctx.font = '10px system-ui'; ctx.textAlign = 'left'; const label = cell.name.length > 27 ? `${cell.name.slice(0, 25)}…` : cell.name;
        const width = Math.min(cell.width * s - 12, ctx.measureText(label).width + 15);
        ctx.fillStyle = selected ? '#4b4930f0' : '#1d2d20ed'; ctx.fillRect(x + 7, y + 7, width, 21);
        ctx.fillStyle = selected ? '#ead9a5' : '#aeb99a'; ctx.save(); ctx.beginPath(); ctx.rect(x + 10, y + 7, width - 5, 21); ctx.clip(); ctx.fillText(label, x + 13, y + 18); ctx.restore(); ctx.textAlign = 'center';
      }
    }
  }
  if ($('show-links').checked) {
    for (const link of project.links) {
      const a = worldPoint(link.a), b = worldPoint(link.b);
      if (a && b) { ctx.strokeStyle = '#bb9ed264'; ctx.setLineDash([3, 5]); ctx.beginPath(); ctx.moveTo(left + a.x * s, top + a.y * s); ctx.lineTo(left + b.x * s, top + b.y * s); ctx.stroke(); ctx.setLineDash([]); }
      drawMarker(a, 'link'); drawMarker(b, 'link');
    }
    if (project.spawn) drawMarker(worldPoint(project.spawn), 'spawn');
  }
  if (!$('link-form').hidden && tool === 'link') {
    for (const side of ['a', 'b']) {
      const endpoint = { cell: $(`link-${side}-cell`).value, x: Number($(`link-${side}-x`).value), y: Number($(`link-${side}-y`).value) };
      if (getCell(project, endpoint.cell)) drawMarker(worldPoint(endpoint), 'link', side.toUpperCase());
    }
  }
  if (validPoint(hover) && ['paint', 'height', 'spawn', 'link'].includes(tool)) {
    const x = Math.floor(hover.x), y = Math.floor(hover.y), size = ['paint', 'height'].includes(tool) ? brush : 1;
    ctx.fillStyle = '#e6d59e17'; ctx.strokeStyle = '#e1cc90aa'; ctx.lineWidth = 1;
    ctx.fillRect(left + x * s, top + y * s, Math.min(size, map.width - x) * s, Math.min(size, map.height - y) * s);
    ctx.strokeRect(left + x * s, top + y * s, Math.min(size, map.width - x) * s, Math.min(size, map.height - y) * s);
  }
  ctx.font = '9px ui-monospace, monospace'; ctx.fillStyle = '#738969'; ctx.textAlign = 'left';
  if (left > 15 && top > 16) ctx.fillText('0, 0', left, top - 11);
  if (left + map.width * s < viewSize.width && top + map.height * s + 13 < viewSize.height) { ctx.textAlign = 'right'; ctx.fillText(`${map.width}, ${map.height}`, left + map.width * s, top + map.height * s + 12); }
  ctx.textAlign = 'center';
}

function chooseTool(value) {
  finishDrag();
  tool = value;
  document.querySelectorAll('[data-tool]').forEach(button => button.classList.toggle('active', button.dataset.tool === tool));
  const help = {
    select: 'Click a cell to select. Shift-click selects more. Double-click to work inside.',
    paint: 'Draw terrain across cell boundaries. A complete brush stroke is one undo step.',
    height: 'Paint elevation, in tile units. Glyph default clears your override. Enable elevation tint to see the shape.',
    link: 'Pick endpoint A, then B. Use the cell menus to reach a detached room. Coordinates are local to each cell.',
    spawn: 'Click a traversable tile to place the player starting point. Cut the world into cells first.',
    pan: 'Drag to move the canvas. Wheel to zoom. Fit restores the whole editing surface.',
  };
  $('tool-help').textContent = help[tool];
  canvas.style.cursor = tool === 'pan' ? 'grab' : tool === 'select' ? 'default' : 'crosshair';
  if (tool === 'height') $('show-heights').checked = true;
  if (tool === 'link' && $('link-form').hidden) openLink();
  scheduleRender();
}
function enterCell(id) {
  if (!getCell(project, id)) return message('Choose a cell or interior first.', 'error');
  activeId = id; selection = new Set([id]); mode = 'cell'; refresh(); fit();
}
function showWorld() { mode = 'world'; refresh(); fit(); }
function openLink() {
  $('link-form').hidden = false; updateOptions($('link-a-cell')); updateOptions($('link-b-cell'));
  syncLinkKind();
  if (activeId) $('link-a-cell').value = activeId;
  const other = allCells().find(c => c.id !== $('link-a-cell').value);
  if (other) $('link-b-cell').value = other.id;
  pendingEndpoint = 'a';
  $('link-name').value = 'New doorway';
  message('Pick endpoint A on the map, or enter both endpoints in the connection form.');
}
function syncLinkKind() {
  const alwaysOpen = $('link-kind').value !== 'door';
  $('link-open').disabled = alwaysOpen;
  if (alwaysOpen) $('link-open').checked = true;
  $('link-open-label').textContent = alwaysOpen ? 'Always open' : 'Start open';
  $('link-open').parentElement.title = alwaysOpen ? 'Stairs and passages stay open; only doors can close.' : 'Choose whether this door starts open or closed.';
}
function applyPaint(point) {
  if (!validPoint(point)) return;
  const map = visibleMap(), startX = Math.floor(point.x), startY = Math.floor(point.y);
  const stamp = `${startX},${startY}`;
  if (drag?.visited?.has(stamp)) return;
  drag?.visited?.add(stamp);
  const detached = map.cell && roomFor(map.cell.id), offsetX = detached ? 0 : map.cell?.x || 0, offsetY = detached ? 0 : map.cell?.y || 0;
  if (tool === 'height') {
    const candidate = clone(project);
    const heights = detached ? getCell(candidate, detached.id).heights : candidate.heights;
    const value = $('height-value').value === 'default' ? null : Number($('height-value').value);
    for (let y = startY; y < Math.min(startY + brush, map.height); y++) for (let x = startX; x < Math.min(startX + brush, map.width); x++) {
      const key = `${x + offsetX},${y + offsetY}`;
      if (value === null) delete heights[key]; else heights[key] = value;
    }
    project = normalizeProject(candidate);
    return;
  }
  if (tool === 'paint' && (mode === 'world' || detached || startX + brush <= map.width && startY + brush <= map.height)) {
    paint(project, startX + offsetX, startY + offsetY, glyph, brush, detached?.id || null);
    return;
  }
  for (let y = startY; y < Math.min(startY + brush, map.height); y++) for (let x = startX; x < Math.min(startX + brush, map.width); x++) {
    paint(project, x + offsetX, y + offsetY, glyph, 1, detached?.id || null);
  }
}
function paintLine(from, to) {
  const length = Math.max(Math.abs(Math.floor(to.x) - Math.floor(from.x)), Math.abs(Math.floor(to.y) - Math.floor(from.y)));
  for (let n = 0; n <= length; n++) applyPaint({ x: from.x + (to.x - from.x) * (length ? n / length : 0), y: from.y + (to.y - from.y) * (length ? n / length : 0) });
}
canvas.addEventListener('pointerdown', event => {
  if (![0, 1].includes(event.button)) return;
  canvas.focus({ preventScroll: true });
  const point = pointFromEvent(event);
  if (event.button === 1 || tool === 'pan' || spaceHeld) {
    event.preventDefault(); canvas.setPointerCapture(event.pointerId); drag = { type: 'pan', px: event.clientX, py: event.clientY, x: camera.x, y: camera.y }; return;
  }
  if (!validPoint(point)) return;
  if (tool === 'select') {
    const c = mode === 'cell' ? activeCell() : cellAt(Math.floor(point.x), Math.floor(point.y));
    if (c) { if (event.shiftKey && mode === 'world') { if (selection.has(c.id)) selection.delete(c.id); else selection.add(c.id); } else selection = new Set([c.id]); activeId = c.id; refresh(); }
    else { selection.clear(); activeId = ''; refresh(); }
  } else if (['paint', 'height'].includes(tool)) {
    canvas.setPointerCapture(event.pointerId); drag = { type: 'paint', before: clone(project), last: point, visited: new Set() };
    try { applyPaint(point); scheduleRender(); } catch (error) { project = drag.before; drag = null; message(error.message, 'error'); scheduleRender(); }
  } else if (tool === 'spawn') {
    const endpoint = endpointAt(point);
    if (!endpoint) return message('Cut the canvas before placing a starting point.', 'error');
    transact('Starting point placed. Players will enter this cell, not the whole atlas.', () => { const p = clone(project); p.spawn = endpoint; project = normalizeProject(p); });
  } else if (tool === 'link') {
    const endpoint = endpointAt(point); if (!endpoint) return message('Cut the canvas before linking cells.', 'error');
    for (const key of ['cell', 'x', 'y']) $(`link-${pendingEndpoint}-${key}`).value = endpoint[key];
    message(`Endpoint ${pendingEndpoint.toUpperCase()} set in ${getCell(project, endpoint.cell).name} at ${endpoint.x}, ${endpoint.y}. ${pendingEndpoint === 'a' ? 'Now pick B, possibly in another room.' : 'Review the connection and choose Create link.'}`);
    if (pendingEndpoint === 'a') pendingEndpoint = 'b'; scheduleRender();
  }
});
canvas.addEventListener('pointermove', event => {
  const point = pointFromEvent(event); hover = point;
  if (drag?.type === 'pan') { camera.x = drag.x + event.clientX - drag.px; camera.y = drag.y + event.clientY - drag.py; }
  else if (drag?.type === 'paint') {
    try { paintLine(drag.last, point); drag.last = point; }
    catch (error) { project = drag.before; drag = null; message(`Stroke cancelled: ${error.message}`, 'error'); refresh(); }
  }
  if (validPoint(point)) {
    const endpoint = endpointAt(point), map = visibleMap();
    $('pointer-status').textContent = `${Math.floor(point.x)}, ${Math.floor(point.y)} · ${map.terrain[Math.floor(point.y)][Math.floor(point.x)]}${endpoint && mode === 'world' ? ` · local ${endpoint.x}, ${endpoint.y}` : ''}`;
  } else $('pointer-status').textContent = `${visibleMap()?.width || 0} × ${visibleMap()?.height || 0} tiles`;
  scheduleRender();
});
function finishDrag() {
  if (drag?.type === 'paint') commit(drag.before, tool === 'height' ? 'Elevation stroke applied.' : 'Terrain stroke applied. Cell boundaries and links stay in place.');
  drag = null;
}
canvas.addEventListener('pointerup', finishDrag);
canvas.addEventListener('pointercancel', () => { if (drag?.type === 'paint') { project = drag.before; refresh(); } drag = null; });
canvas.addEventListener('pointerleave', () => { hover = null; scheduleRender(); });
canvas.addEventListener('dblclick', event => { if (tool !== 'select' || mode !== 'world') return; const p = pointFromEvent(event), cell = cellAt(Math.floor(p.x), Math.floor(p.y)); if (cell) enterCell(cell.id); });
canvas.addEventListener('wheel', event => { event.preventDefault(); const rect = canvas.getBoundingClientRect(); zoom(Math.exp(-event.deltaY * .0015), { x: event.clientX - rect.left, y: event.clientY - rect.top }); }, { passive: false });
canvas.addEventListener('contextmenu', event => event.preventDefault());
document.addEventListener('keydown', event => {
  if (/INPUT|TEXTAREA|SELECT/.test(event.target.tagName) || document.querySelector('dialog[open]')) return;
  if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === 'z') { event.preventDefault(); event.shiftKey ? redo() : undo(); }
  else if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === 'y') { event.preventDefault(); redo(); }
  else if (event.code === 'Space') { event.preventDefault(); spaceHeld = true; }
  else if (/^[1-6]$/.test(event.key)) chooseTool(['select', 'paint', 'height', 'link', 'spawn', 'pan'][Number(event.key) - 1]);
  else if (event.key === 'Escape') { if (drag?.type === 'paint') project = drag.before; drag = null; $('link-form').hidden = true; chooseTool('select'); refresh(); }
  else if (event.key === 'Enter' && tool === 'select' && selection.size === 1) enterCell([...selection][0]);
});
document.addEventListener('keyup', event => { if (event.code === 'Space') spaceHeld = false; });
window.addEventListener('blur', () => { spaceHeld = false; finishDrag(); });

for (const [symbol, name, color] of palette) {
  const button = document.createElement('button'); button.textContent = symbol; button.title = name; button.setAttribute('aria-label', `${name}, glyph ${symbol}`); button.style.color = color; button.classList.toggle('active', symbol === glyph);
  button.addEventListener('click', () => { glyph = symbol; $('terrain-palette').querySelectorAll('button').forEach(b => b.classList.toggle('active', b === button)); chooseTool('paint'); message(`${name} selected. Drag on the canvas to paint.`); });
  $('terrain-palette').append(button);
}
document.querySelectorAll('[data-tool]').forEach(button => button.addEventListener('click', () => chooseTool(button.dataset.tool)));
$('brush-size').addEventListener('change', () => { brush = Number($('brush-size').value); });
for (const id of ['show-cuts', 'show-heights', 'show-links', 'show-territory', 'height-value']) $(id).addEventListener('change', scheduleRender);
$('world-mode').addEventListener('click', showWorld);
$('cell-mode').addEventListener('click', () => enterCell(activeId));
$('drill-down').addEventListener('click', () => mode === 'cell' ? showWorld() : enterCell([...selection][0]));
$('cell-select').addEventListener('change', () => {
  const id = $('cell-select').value;
  if (!id) { selection.clear(); activeId = ''; mode = 'world'; refresh(); return; }
  activeId = id; selection = new Set([id]);
  if (roomFor(id) || mode === 'cell') enterCell(id); else refresh();
});
$('clear-selection').addEventListener('click', () => { selection.clear(); if (mode === 'world') activeId = ''; refresh(); });
$('undo').addEventListener('click', undo); $('redo').addEventListener('click', redo);
$('zoom-in').addEventListener('click', () => zoom(1.2)); $('zoom-out').addEventListener('click', () => zoom(1 / 1.2)); $('fit-map').addEventListener('click', fit);
$('project-name').addEventListener('change', () => transact('Atlas renamed.', () => { const name = $('project-name').value.trim(); if (!name) throw new Error('The atlas needs a name.'); const candidate = clone(project); candidate.name = name; project = normalizeProject(candidate); }));
$('metadata-form').addEventListener('submit', event => {
  event.preventDefault(); const values = { name: $('cell-name').value.trim(), description: $('cell-description').value, z: Number($('cell-z').value), weather: $('cell-weather').value, outdoors: $('cell-outdoors').checked,
    lighting: {artificial: Number($('cell-light').value), daylightAccess: Number($('cell-daylight').value), tone: $('cell-light-tone').value} };
  transact('Cell details updated. Terrain remains on its original coordinates.', () => { const candidate = clone(project), cell = getCell(candidate, [...selection][0]); if (!cell) throw new Error('Select one cell to edit its details.'); Object.assign(cell, values); project = normalizeProject(candidate); });
});
$('territory-form').addEventListener('submit', async event => {
  event.preventDefault();
  const ids = [...selection], territory = {region: $('territory-region').value.trim(), chapter: $('territory-chapter').value,
    claims: [...$('territory-claims').querySelectorAll('input:checked')].map(input => input.value)};
  if (ids.length > 1 && !await askConfirmation('Replace selected territory settings?', `Apply this region, faction-claim set and Chapter site to all ${ids.length} selected cells? Existing territory settings will be replaced; terrain and player visibility will not change.`, 'Apply territory')) return;
  transact('Territory settings saved. Claims and Chapter sites do not alter terrain or player fog.', () => setTerritory(project, ids, territory));
});
$('manage-politics').addEventListener('click', () => { finishDrag(); renderCatalogs(); $('catalog-status').textContent = ''; $('politics-dialog').showModal(); });
$('close-politics').addEventListener('click', () => $('politics-dialog').close());
for (const kind of ['faction', 'chapter']) {
  $(`${kind}-select`).addEventListener('change', () => loadCatalogForm(kind));
  $(`${kind}-form`).addEventListener('submit', event => {
    event.preventDefault();
    const entry = {id: $(`${kind}-id`).value.trim(), name: $(`${kind}-name`).value.trim()};
    if (kind === 'faction') entry.color = $('faction-color').value;
    if (!$(`${kind}-select`).value && project[kind === 'faction' ? 'factions' : 'chapters'].some(item => item.id === entry.id))
      return message('That ID already exists. Select its existing catalog entry to edit it, or choose a new ID.', 'error');
    if (transact(`${kind === 'chapter' ? 'Chapter' : 'Faction'} catalog saved. Assign its territory in the inspector.`, () => (kind === 'faction' ? upsertFaction : upsertChapter)(project, entry))) {
      $(`${kind}-select`).value = entry.id; loadCatalogForm(kind);
    }
  });
  $(`remove-${kind}`).addEventListener('click', async () => {
    const id = $(`${kind}-select`).value;
    if (!id || !await askConfirmation('Delete this catalog entry?', `Delete ${kind} “${id}”? Deletion is refused while any cell or room references it. Undo is available.`, 'Delete entry')) return;
    transact('Unreferenced catalog entry deleted. Undo is available.', () => (kind === 'faction' ? removeFaction : removeChapter)(project, id));
  });
}
$('cut-grid').addEventListener('click', async () => {
  const width = Number($('cut-width').value), height = Number($('cut-height').value);
  if (project.cells.length && !await askConfirmation('Re-cut the world canvas?', 'Terrain stays unchanged and links are remapped, but existing cell names and descriptions may be replaced where boundaries change. Save JSON first to keep another version.', 'Re-cut canvas')) return;
  if (transact('World partitioned. The terrain is unchanged; each rectangle exports as its own playable cell.', () => { cutGrid(project, width, height); selection = new Set(project.cells[0] ? [project.cells[0].id] : []); activeId = [...selection][0] || ''; })) fit();
});
for (const id of ['cut-width', 'cut-height']) $(id).addEventListener('input', () => { $('cut-grid').textContent = Number($('cut-width').value) === 32 && Number($('cut-height').value) === 24 ? 'Cut standard grid' : 'Cut custom grid'; });
$('merge-cells').addEventListener('click', async () => {
  const ids = [...selection], cells = ids.map(id => getCell(project, id));
  if (new Set(cells.map(cell => `${cell.name}\n${cell.description}`)).size > 1 && !await askConfirmation('Merge these cells?', `The merged cell keeps “${cells[0].name}” and its description; the other selected names and descriptions are discarded. Terrain and connections are preserved. Undo is available.`, 'Merge cells')) return;
  transact('Cells merged without moving a terrain tile.', () => { const c = mergeCells(project, ids); selection = new Set([c.id]); activeId = c.id; });
});
$('show-split').addEventListener('click', () => { $('split-form').hidden = !$('split-form').hidden; const c = getCell(project, [...selection][0]); if (c) $('split-offset').value = Math.floor(($('split-axis').value === 'x' ? c.width : c.height) / 2); });
$('split-axis').addEventListener('change', () => { const c = getCell(project, [...selection][0]); if (c) $('split-offset').value = Math.floor(($('split-axis').value === 'x' ? c.width : c.height) / 2); });
$('split-form').addEventListener('submit', event => { event.preventDefault(); if (transact('Cell split. Terrain, links and spawn were remapped in place.', () => { const children = splitCell(project, [...selection][0], $('split-axis').value, Number($('split-offset').value)); selection = new Set(children.map(c => c.id)); activeId = children[0]?.id || ''; })) $('split-form').hidden = true; });
$('add-room').addEventListener('click', () => { let room; if (transact('Detached interior created. Add a doorway or stairs to connect it to another cell.', () => { room = addRoom(project, Number($('room-width').value), Number($('room-height').value), `Interior ${project.rooms.length + 1}`); selection = new Set([room.id]); activeId = room.id; })) enterCell(room.id); });
$('new-link').addEventListener('click', () => { openLink(); chooseTool('link'); $('link-form').scrollIntoView({ behavior: 'smooth', block: 'nearest' }); });
$('cancel-link').addEventListener('click', () => { $('link-form').hidden = true; chooseTool('select'); });
$('link-kind').addEventListener('change', syncLinkKind);
for (const side of ['a', 'b']) {
  $(`pick-${side}`).addEventListener('click', () => { pendingEndpoint = side; chooseTool('link'); const id = $(`link-${side}-cell`).value; if (id) enterCell(id); message(`Click a traversable tile for endpoint ${side.toUpperCase()} in ${getCell(project, id)?.name || 'a cell'}.`); });
  for (const key of ['cell', 'x', 'y']) $(`link-${side}-${key}`).addEventListener('change', scheduleRender);
}
$('link-form').addEventListener('submit', event => {
  event.preventDefault();
  const endpoint = side => ({ cell: $(`link-${side}-cell`).value, x: Number($(`link-${side}-x`).value), y: Number($(`link-${side}-y`).value) });
  const link = { name: $('link-name').value.trim(), kind: $('link-kind').value, open: $('link-open').checked, a: endpoint('a'), b: endpoint('b') };
  if (transact('Reciprocal connection created. The destination remains a separate gameplay cell.', () => addLink(project, link))) { $('link-form').hidden = true; chooseTool('select'); }
});
$('validate-project').addEventListener('click', () => { runValidation(); message(lastValidation.errors.length ? 'Preflight found blocking issues. Review the inspector.' : 'Preflight passed. Export performs the authoritative content validation.', lastValidation.errors.length ? 'error' : 'success'); });

function download(blob, filename) {
  const url = URL.createObjectURL(blob), a = document.createElement('a'); a.href = url; a.download = filename; a.click(); setTimeout(() => URL.revokeObjectURL(url), 1000);
}
function filename() { return (project.name.toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '') || 'ratw-atlas').slice(0, 70); }
$('save-project').addEventListener('click', () => { download(new Blob([JSON.stringify(project, null, 2) + '\n'], { type: 'application/json' }), `${filename()}.atlas.json`); refresh(); message('JSON download requested, not confirmed. Verify that the file finished saving before replacing this canvas.'); });
$('open-project').addEventListener('click', () => $('open-file').click());
$('open-file').addEventListener('change', async event => {
  const file = event.target.files[0]; if (!file) return;
  try {
    if (file.size > 8 * 1024 * 1024) throw new Error('This file exceeds the 8 MB authoring limit.');
    const candidate = normalizeProject(JSON.parse(await file.text()));
    if (dirty && !await askConfirmation('Open a different atlas?', 'Replace the current canvas with this file? Save JSON first to preserve the current version.', 'Open atlas')) return;
    setProject(candidate); draftRecovery = null; $('recovery-banner').hidden = true; saveDraft(); message(`Opened ${file.name}. This has not changed any live game.`);
  } catch (error) { message(`Could not open project: ${error.message}`, 'error'); }
  finally { event.target.value = ''; }
});
$('export-project').addEventListener('click', async () => {
  runValidation();
  if (lastValidation.errors.length) return message('Export blocked. Resolve the issues in Preflight first.', 'error');
  const exportBody = JSON.stringify(project), exportFilename = `${filename()}.world.zip`, exportName = project.name;
  const button = $('export-project'); button.disabled = true; button.textContent = 'Validating…';
  try {
    const sessionResponse = await fetch('/api/session');
    if (!sessionResponse.ok) throw new Error('Cannot reach the local export service. Launch tools/map_editor.py.');
    const session = await sessionResponse.json();
    const response = await fetch('/api/export', { method: 'POST', headers: { 'Content-Type': 'application/json', 'X-RATW-Editor': session.token }, body: exportBody });
    if (!response.ok) {
      let detail; try { detail = await response.json(); } catch { detail = { error: 'The export service rejected this map.' }; }
      throw new Error(Array.isArray(detail.errors) ? detail.errors.join(' · ') : detail.error || detail.message || 'Export validation failed.');
    }
    download(await response.blob(), exportFilename);
    message(`World ZIP requested for the “${exportName}” snapshot at export time. Later edits are not included. Verify the download; a running game is unchanged.`, 'success');
  } catch (error) { message(`Export failed: ${error.message}`, 'error'); }
  finally { button.disabled = false; button.innerHTML = 'Export world <span aria-hidden="true">↗</span>'; }
});
$('new-project').addEventListener('click', () => $('new-dialog').showModal());
$('cancel-new').addEventListener('click', () => $('new-dialog').close());
$('new-form').addEventListener('submit', event => {
  event.preventDefault();
  try {
    const value = createProject(Number($('new-width').value), Number($('new-height').value), $('new-name').value.trim());
    if ($('new-auto-cut').checked) cutGrid(value, 32, 24);
    setProject(value); dirty = true; draftRecovery = null; $('recovery-banner').hidden = true; saveDraft(); refresh(); $('new-dialog').close(); message('Blank atlas ready. Paint continuously, then refine the cuts when the geography feels right.');
  } catch (error) { message(error.message, 'error'); $('new-dialog').close(); }
});
$('recover-draft').addEventListener('click', async () => {
  if (dirty && !await askConfirmation('Restore the earlier draft?', 'Replace this canvas with the earlier local draft? Save JSON first if you want to keep your current edits.', 'Restore draft')) return;
  try { const value = draftRecovery.project; setProject(value); draftRecovery = null; dirty = true; draftSaved = true; $('recovery-banner').hidden = true; refresh(); message('Previous browser-only draft restored. Save JSON for a portable copy; undo history starts here.'); }
  catch (error) { message(`The saved draft could not be restored: ${error.message}`, 'error'); }
});
$('dismiss-draft').addEventListener('click', () => { draftRecovery = null; $('recovery-banner').hidden = true; if (dirty) saveDraft(); refresh(); message('Draft recovery dismissed. Future edits will save this canvas as the local draft.'); });
window.addEventListener('beforeunload', event => { if (dirty && (draftRecovery || !draftSaved)) { event.preventDefault(); event.returnValue = ''; } });

try {
  const stored = localStorage.getItem(DRAFT_KEY);
  if (stored) { const candidate = JSON.parse(stored); if (candidate.project) draftRecovery = { ...candidate, project: normalizeProject(candidate.project) }; }
} catch { message('An older local draft could not be read. Open a saved JSON file to recover it.', 'error'); }
try { setProject(demoProject()); }
catch (error) { setProject(createProject()); message(`Example atlas unavailable: ${error.message}. A blank canvas is ready.`, 'error'); }
if (draftRecovery) $('recovery-banner').hidden = false;
new ResizeObserver(entries => {
  const rect = entries[0].contentRect, wasEmpty = !viewSize.width;
  const previous = viewSize; viewSize = { width: rect.width, height: rect.height };
  const ratio = window.devicePixelRatio || 1;
  canvas.width = Math.round(rect.width * ratio); canvas.height = Math.round(rect.height * ratio); ctx.setTransform(ratio, 0, 0, ratio, 0, 0);
  if (wasEmpty) fit(); else { camera.x += (rect.width - previous.width) / 2; camera.y += (rect.height - previous.height) / 2; scheduleRender(); }
}).observe($('canvas-wrap'));
