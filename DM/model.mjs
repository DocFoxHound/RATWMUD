// Presentation helpers only. The DM service and native authority validate every effect.
export const EXECUTABLE = new Set(['notice', 'weather', 'npc_relocate', 'economy_transfer']);
export const EVENT_LABELS = Object.freeze({notice:'Narrative notice',weather:'Weather change',npc_relocate:'NPC relocation',economy_transfer:'Economy transfer',brigands:'Brigands',assassins:'Assassination',war:'Army / war','faction-collapse':'Faction collapse'});
export const array = value => Array.isArray(value) ? value : [];
export const string = value => typeof value === 'string' ? value : typeof value === 'number' && Number.isFinite(value) ? String(value) : '';
export const finite = (value, fallback = 0) => Number.isFinite(Number(value)) && value !== null && value !== '' ? Number(value) : fallback;
export const recordId = value => string(value?.id);
export const recordName = value => string(value?.name || value?.title || value?.id) || 'Unnamed record';
export const cellOf = value => string(value?.cell || value?.cellId);
export const uniqueIds = text => [...new Set(string(text).split(/[\s,]+/).map(v => v.trim()).filter(Boolean))];

export function utcDate(value) {
  if (value === null || value === undefined || value === '') return null;
  const date = new Date(typeof value === 'number' ? value * 1000 : value);
  return Number.isFinite(date.getTime()) ? date : null;
}
export function utcLabel(value) {
  const date = utcDate(value);
  return date ? `${date.toISOString().replace('T', ' ').slice(0, 19)} UTC` : 'Not reported';
}
export function inputUtc(value) {
  const date = utcDate(value);
  return date ? date.toISOString().slice(0, 16) : '';
}
export function parseUtcInput(value) {
  if (!/^\d{4}-\d\d-\d\dT\d\d:\d\d(?::\d\d)?$/.test(value)) throw new Error('Enter a valid UTC date and time.');
  const date = new Date(`${value}Z`);
  if (!Number.isFinite(date.getTime()) || date.toISOString().slice(0, 16) !== value.slice(0, 16)) throw new Error('That UTC date is not valid.');
  return date.toISOString();
}
export function integer(value, name, min, max) {
  if (value === '' || value === null || value === undefined) throw new Error(`${name} is required.`);
  const number = Number(value);
  if (!Number.isSafeInteger(number) || number < min || number > max) throw new Error(`${name} must be a whole number between ${min} and ${max}.`);
  return number;
}
export function numeric(value, name, min, max) {
  if (value === '' || value === null || value === undefined) throw new Error(`${name} is required.`);
  const number = Number(value);
  if (!Number.isFinite(number) || number < min || number > max) throw new Error(`${name} must be between ${min} and ${max}.`);
  return number;
}
export function bridgeFresh(state, receivedAt, now = Date.now()) {
  if (!state || !state.bridge || state.bridge.live !== true || state.bridge.stale === true || now - receivedAt > 10000) return false;
  const age = finite(state.bridge.ageSeconds, Number.POSITIVE_INFINITY);
  if (age < 0 || age + Math.max(0, now - receivedAt) / 1000 > 10) return false;
  return true;
}
export function normalizeCells(snapshot) {
  return array(snapshot?.cells).filter(cell => recordId(cell)).map(cell => ({...cell, id:recordId(cell), name:recordName(cell), x:finite(cell.x ?? cell.worldX), y:finite(cell.y ?? cell.worldY), z:finite(cell.z ?? cell.worldZ), width:Math.max(1,finite(cell.width,1)), height:Math.max(1,finite(cell.height,1))}));
}
export function characters(state) {
  return array(state?.snapshot?.characters).filter(value => recordId(value));
}
export function mergedRecords(primary, fallback) {
  const values = new Map(array(fallback).map(value => [recordId(value), value]));
  for (const value of array(primary)) if (recordId(value)) values.set(recordId(value), {...values.get(recordId(value)), ...value});
  return [...values.values()].filter(value => recordId(value));
}
export function statusClass(status) {
  const value = string(status).toLowerCase();
  if (/fail|reject|expired|blocked/.test(value)) return 'error';
  if (/queue|approv|pending|relocat|scheduled/.test(value)) return 'warn';
  if (/applied|arrived|complete|live|online/.test(value)) return '';
  if (/plan|draft/.test(value)) return 'info';
  return 'neutral';
}
export function eventPayload(kind, values) {
  switch (kind) {
    case 'notice': {
      const payload = {scope:values.scope, text:string(values.text).trim()};
      if (!payload.text) throw new Error('Write the notice text.');
      if (payload.scope !== 'world') {
        if (!values.target) throw new Error('Choose a notice target.');
        payload.target = values.target;
      }
      return payload;
    }
    case 'weather':
      if (!values.cell) throw new Error('Choose a cell for this weather change.');
      return {cell:values.cell,preset:values.preset};
    case 'npc_relocate':
      if (!values.npc || !values.cell) throw new Error('Choose an existing NPC and destination cell.');
      return {npc:values.npc,cell:values.cell,x:numeric(values.x,'Destination X',0,256),y:numeric(values.y,'Destination Y',0,256)};
    case 'economy_transfer': {
      if (!values.from || !values.to || values.from === values.to) throw new Error('Choose two different existing accounts.');
      const payload = {from:values.from,to:values.to,item:values.item || '',quantity:integer(values.quantity,'Quantity',0,99),coins:integer(values.coins,'Coins',0,1000000)};
      if (!payload.coins && !payload.quantity) throw new Error('Transfer at least one coin or item.');
      if (payload.quantity && !payload.item) throw new Error('Choose the item being transferred.');
      if (payload.item && !payload.quantity) throw new Error('Set a positive quantity for the selected item, or select coins only.');
      return payload;
    }
    default: throw new Error('This effect has no executor. Save it as a planned story beat instead.');
  }
}

export function auditStatus(item) {
  if (typeof item?.status === 'string' && item.status) return item.status;
  const action = string(item?.action);
  if (action.startsWith('rejected:') || item?.ok === false) return 'rejected';
  if (action.startsWith('event.')) {
    const status = action.slice(6);
    if (['applied','failed','queued','scheduled','blocked','cancelled'].includes(status)) return status;
  }
  return 'recorded';
}

export function commandMessage(action, result = {}) {
  const planning = {
    'campaign.upsert':'Campaign saved to Storykeeper. No world effect was executed.',
    'beat.upsert':'Story beat saved as a plan. No encounter or world effect was executed.',
    'chapter.upsert':'Chapter profile saved. Settlement sites and declared capacity are planning metadata, not constructed buildings.',
    'faction.upsert':'Faction record saved. No residents or armies were created.',
    'opinion.set':'Faction opinion and its reason were recorded in Storykeeper.',
    'campaign.delete':'Campaign deleted from Storykeeper.',
    'beat.delete':'Story beat deleted from Storykeeper.',
    'migration.preview':'Migration preview generated. Review capacity and consequences; nobody has been moved.',
    'chapter.peak':result.suggestion ? 'An observed playtime suggestion is ready. Choose it explicitly; nothing has been scheduled.' : string(result.reason) || 'No observed playtime suggestion is available.',
  };
  if (planning[action]) return planning[action];
  if (action === 'event.create') return result.status === 'blocked' ? 'Story plan saved. Its executor is unavailable; nothing was queued.' : 'Event draft saved. It requires a separate approval before execution.';
  if (action === 'event.cancel') return 'Undispatched event cancelled. No applied effect was rolled back.';
  if (action === 'migration.approve') return 'Migration approved and queued. Final adoption and source-faction consequences wait for verified arrival.';
  if (action === 'event.approve') return result.status === 'scheduled' ? 'Event approved for its fixed UTC schedule. It has not been applied.' : 'Event approved and queued. Await authoritative confirmation; queued is not applied.';
  return string(result.message || result.detail) || 'Operator request recorded. Review its authoritative status.';
}

// Terrain: the one catalog (Data/Terrain/terrain.json, generated as terrain.generated.mjs). Cells store each tile as
// its ASCII code; the map draws the catalog glyph (or its plain ASCII fallback) in the tile's colours.
const TERRAIN_FALLBACK = Object.freeze({glyph:'?',ascii:'?',name:'Unknown terrain',fg:'#637c60',bg:''});
/** The catalog's tiles by stored code (an empty lookup when no catalog could be loaded). */
export function terrainLookup(tiles) {
  return new Map(array(tiles).filter(t => t && typeof t.code === 'string' && t.code.length === 1).map(t => [t.code, t]));
}
/** What the map draws for a stored code: {text, fg, bg, name}. Unknown codes (or no catalog) draw the code itself. */
export function terrainDraw(lookup, code, ascii = false) {
  const tile = lookup instanceof Map ? lookup.get(code) : undefined;
  if (!tile) return {text:string(code) || TERRAIN_FALLBACK.glyph, fg:TERRAIN_FALLBACK.fg, bg:TERRAIN_FALLBACK.bg, name:TERRAIN_FALLBACK.name};
  const glyph = string(tile.glyph) || string(tile.ascii) || code;
  // U+FE0E asks for text presentation, so symbols such as ♨ ⚒ never draw as emoji.
  return {text:ascii ? string(tile.ascii) || code : (glyph.codePointAt(0) >= 0x2600 ? glyph + '︎' : glyph), fg:string(tile.fg) || TERRAIN_FALLBACK.fg, bg:string(tile.bg), name:string(tile.name)};
}
