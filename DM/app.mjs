import {EXECUTABLE, EVENT_LABELS, array, string, finite, recordId, recordName, cellOf, uniqueIds, utcLabel, utcDate, inputUtc, parseUtcInput, integer, numeric, bridgeFresh, normalizeCells, characters, mergedRecords, statusClass, eventPayload, auditStatus, commandMessage} from './model.mjs';

const $ = selector => document.querySelector(selector);
const $$ = selector => [...document.querySelectorAll(selector)];
const blankState = () => ({snapshot:null,campaigns:[],beats:[],events:[],chapters:[],factions:[],opinions:[],activity:[],routes:[],migrationPreviews:[],audit:[]});
let state = blankState(), token = '', receivedAt = 0, epoch = 0, polling = false, busy = false, transportError = '', uncertain = null;
let selectedCell = '', selectedChapter = '', selectedPreview = '', peakSuggestion = null, confirmation = null, currentPage = 'world';
let statusNotice = null;
const renderSignatures = new Map();
let map = {zoom:1,panX:0,panY:0,scale:1,offsetX:0,offsetY:0,width:0,height:0,drag:null,moved:false,hitCells:[]};

function node(tag, className = '', text = '') {
  const element = document.createElement(tag);
  if (className) element.className = className;
  if (text !== '') element.textContent = text;
  return element;
}
function button(text, action, options = {}) {
  const element = node('button', options.className || '', text);
  element.type = 'button';
  if (options.write) element.dataset.write = '';
  if (options.disabled) element.dataset.unavailable = 'true';
  if (options.key) element.dataset.focusKey = options.key;
  element.addEventListener('click', () => Promise.resolve().then(action).catch(showError));
  return element;
}
function badge(text) { return node('span', `badge ${statusClass(text)}`, text || 'unreported'); }
function empty(container, text) { container.replaceChildren(node('p', 'empty-copy', text)); }
function showMessage(text, error = false) {
  statusNotice = null;
  const element = $('#message');
  element.replaceChildren(node('span', '', text));
  element.hidden = false;
  element.classList.toggle('error', error);
  if (uncertain) element.append(button('Resolve uncertain request', retryUncertain, {className:'text-button'}));
}
function showError(error) { showMessage(error?.message || String(error), true); }
function changed(key, value) {
  const signature = JSON.stringify(value);
  if (renderSignatures.get(key) === signature) return false;
  renderSignatures.set(key, signature);
  return true;
}
function preserveInteraction(renderContents) {
  const active = document.activeElement, focusKey = active?.dataset.focusKey;
  const scroll = [document.scrollingElement,...$$('.cell-list,.detail-panel,.table-scroll,.map-inspector')].filter(Boolean).map(element=>[element,element.scrollLeft,element.scrollTop]);
  const openDetails = new Set($$('details[open][data-disclosure-key]').map(element=>element.dataset.disclosureKey));
  renderContents();
  for (const detail of $$('details[data-disclosure-key]')) if (openDetails.has(detail.dataset.disclosureKey)) detail.open=true;
  if (focusKey && !active.isConnected) {
    const replacement=$$('[data-focus-key]').find(element=>element.dataset.focusKey===focusKey);
    if(replacement&&!replacement.disabled)replacement.focus({preventScroll:true});
  }
  for(const [element,x,y] of scroll)if(element.isConnected){element.scrollLeft=x;element.scrollTop=y;}
}
function updateEventNotice() {
  if(!statusNotice)return;
  const event=array(state.events).find(value=>value.id===statusNotice.id);
  if(!event||event.status===statusNotice.status&&event.detail===statusNotice.detail)return;
  // An in-flight GET may predate the accepted command. Never regress its toast.
  if(statusNotice.status==='queued'&&['draft','scheduled'].includes(event.status)||statusNotice.status==='scheduled'&&event.status==='draft')return;
  const title=string(event.title)||'Event';
  const message=event.status==='applied'?`${title}: verified applied. ${string(event.detail)}`
    :event.status==='failed'||event.status==='blocked'?`${title}: ${event.status}. ${string(event.detail)}`
    :`${title}: ${event.status}. ${string(event.detail)}`;
  showMessage(message,event.status==='failed'||event.status==='blocked');
  if(!['applied','failed','blocked','cancelled'].includes(event.status))statusNotice={id:event.id,status:event.status,detail:event.detail};
}
function cells() { return normalizeCells(state.snapshot); }
function chapters() { return mergedRecords(state.chapters, state.snapshot?.chapters); }
function factions() { return mergedRecords(state.factions, state.snapshot?.factions); }
function nameFor(list, id) { return recordName(list.find(value => recordId(value) === string(id)) || {id}); }
function playerName(id) { return nameFor(characters(state), id); }
function cellName(id) { return nameFor(cells(), id); }
function chapterMembers(chapter) { return array(chapter?.memberIds || chapter?.members).map(value => typeof value === 'string' ? value : recordId(value)).filter(Boolean); }
function chapterCells(chapter) { return array(chapter?.cellIds || chapter?.cells).map(value => typeof value === 'string' ? value : recordId(value)).filter(Boolean); }
function ownActivity(chapter) { return array(state.activity).filter(item => item.chapter === chapter.id || item.chapterId === chapter.id || chapterMembers(chapter).includes(string(item.characterId || item.playerId))); }
function canWrite() { return Boolean(token && !transportError && !busy && !uncertain && bridgeFresh(state, receivedAt)); }
function updateGuards() {
  const fresh = Boolean(token && !transportError && bridgeFresh(state, receivedAt));
  for (const element of $$('[data-write]')) element.disabled = !fresh || busy || Boolean(uncertain) || element.dataset.unavailable === 'true';
  $('#refresh').disabled = !token || polling;
  $('#connect').hidden = Boolean(token);
  $('#disconnect').hidden = !token;
  const status = $('#connection-status');
  status.textContent = !token ? 'Disconnected' : fresh ? 'Live authority' : 'Read-only · stale';
  status.className = `badge ${fresh ? '' : token ? 'warn' : 'neutral'}`;
  const banner = $('#connection-banner');
  banner.classList.toggle('live', fresh);
  let heading, detail;
  if (!token) {
    heading = 'No operator session connected.';
    detail = 'Start the separate DM service, then open its authorized operator URL or connect with its session token. No live world has been loaded.';
  } else if (fresh) {
    heading = 'Connected to the authoritative world.';
    detail = 'This is an operator-only, whole-world view. Draft events do nothing until explicitly approved; queued actions are not yet applied.';
  } else {
    heading = 'Read-only: authoritative world data is unavailable or stale.';
    detail = transportError || string(state.bridge?.detail) || 'The live snapshot is more than ten seconds old. Cached data remains visible; all changes are disabled.';
  }
  banner.replaceChildren(node('strong','',heading), node('span','',detail));
  if (token && !fresh) $('#world-canvas').classList.add('stale-data'); else $('#world-canvas').classList.remove('stale-data');
  const age = state.snapshot?.generatedAtUnix ? Math.max(0, Date.now()/1000-finite(state.snapshot.generatedAtUnix)) : null;
  $('#last-snapshot').textContent = age === null ? 'No server snapshot' : `Last snapshot ${Math.floor(age)}s ago · sequence ${string(state.snapshot.sequence) || '—'}`;
  $('#server-clock').textContent = state.serverTime ? `Server ${utcLabel(state.serverTime)}` : 'Server UTC unavailable';
}
async function api(path, options = {}) {
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), 8000);
  try {
    const response = await fetch(path, {...options,signal:controller.signal,cache:'no-store',credentials:'omit',headers:{Authorization:`Bearer ${token}`,...(options.body ? {'Content-Type':'application/json'} : {}),...options.headers}});
    let body;
    try { body = await response.json(); } catch { throw new Error(`The service returned a non-JSON response (${response.status}).`); }
    if (!response.ok || body?.ok === false) {
      const error = new Error(string(body?.error?.message || body?.error || body?.message) || `The service rejected the request (${response.status}).`);
      error.rejected = true;
      throw error;
    }
    return body;
  } finally { clearTimeout(timeout); }
}
async function refreshState() {
  if (!token || polling) return;
  const requestEpoch = epoch;
  polling = true;
  updateGuards();
  try {
    const body = await api('/api/state');
    if (requestEpoch !== epoch) return;
    if (!body || body.version !== 1 || typeof body.snapshot !== 'object') throw new Error('The service returned an incompatible state document.');
    state = body;
    receivedAt = Date.now();
    transportError = '';
    render();
  } catch (error) {
    if (requestEpoch === epoch) transportError = error.name === 'AbortError' ? 'The service did not respond. Cached data is read-only.' : error.message;
  } finally {
    polling = false;
    updateGuards();
  }
}
async function connectSession(sessionToken) {
  const value = string(sessionToken).trim();
  if (!value) throw new Error('Enter the session token from your DM service.');
  epoch++;
  token = value;
  receivedAt = 0;
  state = blankState();
  renderSignatures.clear();
  uncertain = null;
  transportError = 'Connecting to the operator service…';
  $('#session-token').value = '';
  $('#session-dialog').close();
  render();
  await refreshState();
}
function disconnectSession() {
  epoch++;
  token = '';
  state = blankState();
  renderSignatures.clear();
  receivedAt = 0;
  selectedCell = selectedChapter = selectedPreview = '';
  peakSuggestion = null;
  uncertain = null;
  transportError = '';
  if (confirmation) finishConfirmation(false);
  render();
  showMessage('Disconnected. The bearer token and cached world data have been cleared from this page.');
}
function confirmAction(title, copy, payload, label = 'Confirm') {
  if (confirmation) return Promise.resolve(false);
  $('#confirm-title').textContent = title;
  $('#confirm-copy').textContent = copy;
  $('#confirm-payload').textContent = JSON.stringify(payload, null, 2);
  $('#confirm-accept').textContent = label;
  $('#confirm-dialog').showModal();
  return new Promise(resolve => { confirmation = resolve; });
}
function finishConfirmation(accepted) {
  const resolve = confirmation;
  confirmation = null;
  $('#confirm-dialog').close();
  resolve?.(accepted);
}
async function sendCommand(action, payload, options = {}) {
  if (!canWrite()) throw new Error('Changes require a fresh authority snapshot and a connected operator session.');
  if (options.confirm !== false && !await confirmAction(options.title || 'Review operator action', options.copy || 'Review the values below before writing this operator record.', options.review ?? payload, options.label || 'Confirm')) return null;
  if (!canWrite()) throw new Error('The connection became stale while reviewing. Refresh before confirming again.');
  return runCommand({id:crypto.randomUUID(),action,payload}, options.onResult);
}
async function runCommand(command, onResult) {
  busy = true;
  updateGuards();
  const requestEpoch = epoch;
  try {
    const result = await api('/api/command', {method:'POST',body:JSON.stringify(command)});
    if (requestEpoch !== epoch) return null;
    uncertain = null;
    const data = result.result ?? result;
    await onResult?.(data);
    showMessage(commandMessage(command.action,data));
    const event=data?.event||data;
    if((command.action.startsWith('event.')||command.action==='migration.approve')&&event?.id&&event?.status&&!['applied','failed','blocked','cancelled'].includes(event.status))
      statusNotice={id:event.id,status:event.status,detail:event.detail};
    await refreshState();
    return data;
  } catch (error) {
    if (requestEpoch !== epoch) return null;
    if (!error.rejected) {
      uncertain = {command,onResult};
      showMessage(`The outcome is unknown for request ${command.id}. Resolve this same request before creating another change; it will be retried with the identical idempotency ID.`, true);
    } else {
      uncertain = null;
      showError(error);
    }
    return null;
  } finally { busy = false; updateGuards(); }
}
async function retryUncertain() {
  if (!uncertain || busy || !token) return;
  if (transportError || !bridgeFresh(state,receivedAt)) throw new Error('Reconnect to a fresh authority snapshot before resolving this request. The original request ID is retained.');
  const retry = uncertain;
  if (!await confirmAction('Resolve an uncertain request', 'This resends the original command with exactly the same ID. The service will return the stored result if it already accepted it.', retry.command, 'Resolve safely')) return;
  await runCommand(retry.command, retry.onResult);
}

function setPage(page) {
  if (!$(`#page-${page}`)) return;
  currentPage = page;
  for (const element of $$('.page')) element.hidden = element.id !== `page-${page}`;
  for (const element of $$('[data-page]')) {
    const selected = element.dataset.page === page;
    element.classList.toggle('active', selected);
    if (selected) element.setAttribute('aria-current','page'); else element.removeAttribute('aria-current');
  }
  if (page === 'world') requestAnimationFrame(drawMap);
}
function selectOptions(select, values, optional = false) {
  if (!select) return;
  const selected = select.value;
  const options = values.map(value => ({id:recordId(value),name:recordName(value)}));
  const signature = JSON.stringify([optional,options]);
  if (select.dataset.signature === signature) return;
  select.dataset.signature = signature;
  select.replaceChildren();
  const placeholder = node('option','',optional ? 'Unassigned' : options.length ? 'Choose…' : 'No records available');
  placeholder.value = '';
  select.append(placeholder);
  for (const option of options) {
    const element = node('option','',`${option.name}${option.name !== option.id ? ` · ${option.id}` : ''}`);
    element.value = option.id;
    select.append(element);
  }
  if (options.some(value => value.id === selected)) select.value = selected;
}
function optionSources() {
  const actors = characters(state);
  return {cells:cells(),chapters:chapters(),factions:factions(),campaigns:array(state.campaigns),beats:array(state.beats),players:actors.filter(actor=>!actor.npc),npcs:actors.filter(actor=>actor.npc),accounts:mergedRecords(state.snapshot?.accounts, actors)};
}
function renderOptions() {
  const sources = optionSources();
  for (const select of $$('select[data-options]')) selectOptions(select,sources[select.dataset.options] || [],select.hasAttribute('data-optional'));
  renderOpinionTarget();
  const levels = [...new Set(cells().map(cell=>cell.z))].sort((a,b)=>a-b);
  const level = $('#map-level'), current = level.value;
  if (level.dataset.signature !== JSON.stringify(levels)) {
    level.replaceChildren(node('option','','All elevations'));
    level.firstChild.value = 'all';
    for (const z of levels) { const option=node('option','',`Z ${z}`); option.value=String(z); level.append(option); }
    if (levels.includes(Number(current)) && current !== 'all') level.value=current;
    level.dataset.signature=JSON.stringify(levels);
  }
  renderNoticeTarget();
}
function definitionList(values) {
  const list = node('dl','definition-list');
  for (const [key,value] of values) { list.append(node('dt','',key),node('dd','',string(value) || 'Not reported')); }
  return list;
}
function details(value, label = 'Record details', key = '') {
  const element = node('details');
  const summary=node('summary','',label);
  if(key){element.dataset.disclosureKey=key;summary.dataset.focusKey=`disclosure:${key}`;}
  element.append(summary,node('pre','',JSON.stringify(value,null,2)));
  return element;
}
function record(title, subtitle, status) {
  const element = node('div','record'), header=node('div','record-head'), text=node('div');
  text.append(node('div','record-title',title));
  if (subtitle) text.append(node('div','record-subtitle',subtitle));
  header.append(text);
  if (status) header.append(badge(status));
  element.append(header);
  return element;
}
function metrics(container, values) {
  if(!changed(`metrics:${container.id}`,values))return;
  container.replaceChildren(...values.map(([number,label])=>{
    const item=node('div','metric'); item.append(node('strong','',string(number)),node('span','',label)); return item;
  }));
}
function render() {
  preserveInteraction(()=>{renderOptions(); renderWorld(); renderCharacters(); renderChapters(); renderFactions(); renderStories(); renderEvents(); renderMigration(); renderAudit(); updateGuards();updateEventNotice();});
}

function worldClaims(cell) {
  return array(cell.territory?.claims).map(value=>typeof value === 'string' ? value : string(value.factionId || value.id || value.faction)).filter(Boolean);
}
function chaptersAt(cell) {
  return chapters().filter(chapter=>chapterCells(chapter).includes(cell.id) || cell.territory?.chapter===chapter.id);
}
function renderWorld() {
  const world=cells(), actors=characters(state), filter=$('#cell-search').value.toLowerCase();
  $('#world-summary').textContent = world.length ? `${world.length} cells · ${chapters().length} Chapters` : 'Awaiting data';
  $('#cell-count').textContent=String(world.length);
  $('#map-empty').hidden=world.length>0;
  const list=$('#cell-list');
  const filtered=world.filter(cell=>`${cell.id} ${cell.name}`.toLowerCase().includes(filter));
  if(changed('cell-list',[filtered.map(cell=>[cell.id,cell.name,cell.width,cell.height,cell.z]),filter])){
    list.replaceChildren();
    for (const cell of filtered) {
      const item=button('',()=>{selectedCell=cell.id;renderWorld();},{className:'cell-button',key:`cell:${cell.id}`});
      item.dataset.cellId=cell.id;
      item.append(node('span','',cell.name),node('small','',`${cell.width}×${cell.height} · Z${cell.z}`));list.append(item);
    }
    if (!filtered.length) empty(list,world.length?'No matching cells.':'Cells appear after a live snapshot.');
  }
  for(const item of list.querySelectorAll('[data-cell-id]')){const selected=item.dataset.cellId===selectedCell;item.classList.toggle('selected',selected);item.setAttribute('aria-pressed',String(selected));}
  const detail=$('#cell-detail'), cell=world.find(value=>value.id===selectedCell);
  if(changed('cell-detail',[cell?.id,cell?.name,cell?.description,cell?.x,cell?.y,cell?.z,cell?.outdoors,cell?.territory,factions(),chapters(),actors.filter(actor=>cellOf(actor)===selectedCell).length])){
    detail.replaceChildren();
    if (cell) {
      detail.append(node('h4','',cell.name),definitionList([['ID',cell.id],['Location',`${cell.x}, ${cell.y}, Z ${cell.z}`],['Terrain',cell.outdoors?'Outdoors':'Sheltered'],['Region',string(cell.territory?.region)||'Unassigned'],['Claims',worldClaims(cell).map(id=>nameFor(factions(),id)).join(', ')||'None'],['Chapters',chaptersAt(cell).map(recordName).join(', ')||'None'],['Reported residents',actors.filter(actor=>cellOf(actor)===cell.id).length]]));
      if (cell.description) detail.append(node('p','',string(cell.description)));
    } else empty(detail,'Select a cell to inspect metadata, residents, and claims.');
  }
  metrics($('#world-metrics'),[[state.snapshot?world.length:'—','Independently stored cells'],[state.snapshot?actors.filter(actor=>!actor.npc&&actor.online).length:'—','Players online'],[state.snapshot?actors.filter(actor=>actor.npc).length:'—','Reported NPC residents'],[token?chapters().length:'—','Chapter records']]);
  drawMap();
}
function fitMap() { map.zoom=1;map.panX=map.panY=0;drawMap(); }
function drawMap() {
  if (currentPage!=='world') return;
  const canvas=$('#world-canvas'), bounds=canvas.getBoundingClientRect();
  if (!bounds.width || !bounds.height) return;
  const ratio=Math.min(2,window.devicePixelRatio||1);
  if (canvas.width!==Math.round(bounds.width*ratio)||canvas.height!==Math.round(bounds.height*ratio)) {canvas.width=Math.round(bounds.width*ratio);canvas.height=Math.round(bounds.height*ratio);}
  const ctx=canvas.getContext('2d');
  ctx.setTransform(ratio,0,0,ratio,0,0);ctx.clearRect(0,0,bounds.width,bounds.height);
  ctx.strokeStyle='#2c42312e';ctx.lineWidth=1;
  for(let x=0;x<bounds.width;x+=24){ctx.beginPath();ctx.moveTo(x,0);ctx.lineTo(x,bounds.height);ctx.stroke();}
  for(let y=0;y<bounds.height;y+=24){ctx.beginPath();ctx.moveTo(0,y);ctx.lineTo(bounds.width,y);ctx.stroke();}
  const level=$('#map-level').value, world=cells().filter(cell=>level==='all'||String(cell.z)===level);
  map.hitCells=[];
  if(!world.length)return;
  const minX=Math.min(...world.map(c=>c.x)),minY=Math.min(...world.map(c=>c.y)),maxX=Math.max(...world.map(c=>c.x+c.width)),maxY=Math.max(...world.map(c=>c.y+c.height));
  const base=Math.min((bounds.width-70)/Math.max(1,maxX-minX),(bounds.height-88)/Math.max(1,maxY-minY));
  map.scale=Math.max(.02,base)*map.zoom;
  map.offsetX=(bounds.width-(maxX-minX)*map.scale)/2-minX*map.scale+map.panX;
  map.offsetY=(bounds.height-(maxY-minY)*map.scale)/2-minY*map.scale+map.panY;
  map.width=bounds.width;map.height=bounds.height;
  const positions=new Map();
  for(const cell of [...world].sort((a,b)=>a.z-b.z)){
    const shift=level==='all'?cell.z*10:0;
    const x=map.offsetX+cell.x*map.scale+shift,y=map.offsetY+cell.y*map.scale-shift,w=cell.width*map.scale,h=cell.height*map.scale;
    positions.set(cell.id,{x,y,w,h});map.hitCells.push({id:cell.id,x,y,w,h});
    ctx.fillStyle=cell.outdoors?'#24382a':'#302f24';ctx.fillRect(x,y,w,h);
    const claims=worldClaims(cell), belongs=chaptersAt(cell);
    if($('#layer-claims').checked&&claims.length){
      const faction=factions().find(value=>value.id===claims[0]);
      ctx.fillStyle=/^#[0-9a-f]{6}$/i.test(faction?.color||'')?`${faction.color}32`:'#927aac33';ctx.fillRect(x,y,w,h);
      if(claims.length>1){ctx.save();ctx.beginPath();ctx.rect(x,y,w,h);ctx.clip();ctx.strokeStyle='#b6a1c75f';for(let d=-h;d<w;d+=12){ctx.beginPath();ctx.moveTo(x+d,y+h);ctx.lineTo(x+d+h,y);ctx.stroke();}ctx.restore();}
    }
    if(map.scale>=5){
      ctx.font=`${Math.min(14,map.scale*.88)}px ui-monospace,monospace`;ctx.textAlign='center';ctx.textBaseline='middle';
      for(let ty=0;ty<Math.min(cell.height,256);ty++){
        const row=string(array(cell.terrain)[ty]);
        for(let tx=0;tx<Math.min(cell.width,256);tx++){
          const glyph=row[tx];if(!glyph||glyph===' ')continue;
          ctx.fillStyle=glyph==='#'?'#829078':glyph==='~'?'#62868b':glyph==='+'?'#c5ab6f':'#637c60';
          ctx.fillText(glyph,x+(tx+.5)*map.scale,y+(ty+.5)*map.scale);
        }
      }
    }
    ctx.strokeStyle=selectedCell===cell.id?'#e0c585':'#7b8964';ctx.lineWidth=selectedCell===cell.id?2.5:1;ctx.strokeRect(x+.5,y+.5,w-1,h-1);
    if($('#layer-chapters').checked&&belongs.length){ctx.strokeStyle='#b9a1cf';ctx.lineWidth=2;ctx.strokeRect(x+4,y+4,Math.max(0,w-8),Math.max(0,h-8));}
    if(w>55&&h>25){ctx.fillStyle='#101b14db';ctx.fillRect(x+4,y+4,Math.min(w-8,Math.max(50,cell.name.length*5.9+10)),19);ctx.font='10px ui-sans-serif,system-ui';ctx.textAlign='left';ctx.textBaseline='middle';ctx.fillStyle='#d8dcc0';ctx.fillText(cell.name,x+9,y+14,Math.max(10,w-18));}
  }
  if($('#layer-actors').checked){
    for(const actor of characters(state)){
      if(!actor.npc&&!actor.online)continue;
      const pos=positions.get(cellOf(actor));if(!pos)continue;
      const x=pos.x+finite(actor.x??actor.position?.x)*map.scale,y=pos.y+finite(actor.y??actor.position?.y)*map.scale;
      ctx.fillStyle=actor.npc?'#98bbaa':'#e8cc8a';ctx.strokeStyle='#111b13';ctx.lineWidth=2;
      if(map.scale>=5){ctx.font=`bold ${Math.max(10,Math.min(15,map.scale*.65))}px ui-monospace,monospace`;ctx.textAlign='center';ctx.textBaseline='middle';ctx.strokeText('W',x,y);ctx.fillText('W',x,y);}
      else{ctx.beginPath();ctx.arc(x,y,actor.npc?2.3:3,0,Math.PI*2);ctx.fill();}
    }
  }
  $('#map-coordinate').textContent=`${Math.round(map.zoom*100)}% · ${level==='all'?'all elevations':`Z ${level}`}`;
}

function renderCharacters(){
  const filter=$('#character-filter').value, search=$('#character-search').value.toLowerCase(), rows=$('#character-rows');
  const actors=characters(state).filter(actor=>filter==='all'||filter==='npc'&&actor.npc||filter==='online'&&!actor.npc&&actor.online||filter==='offline'&&!actor.npc&&!actor.online).filter(actor=>`${recordName(actor)} ${actor.id} ${cellName(cellOf(actor))}`.toLowerCase().includes(search));
  if(!changed('character-rows',[actors,chapters(),cells().map(cell=>[cell.id,cell.name]),filter,search]))return;
  rows.replaceChildren();
  for(const actor of actors){
    const row=node('tr'), identity=node('td'), presence=node('td'), location=node('td'), standing=node('td'), activity=node('td');
    identity.append(node('strong','',recordName(actor)),node('small','',actor.id));
    presence.append(badge(actor.npc?'NPC':actor.online?'online':'offline'));
    if(actor.active)presence.append(node('small','','Recently active'));
    location.append(node('span','',cellName(cellOf(actor))),node('small','',`${finite(actor.x??actor.position?.x).toFixed(1)}, ${finite(actor.y??actor.position?.y).toFixed(1)}${!actor.npc&&!actor.online?' · last known':''}`));
    const memberships=chapters().filter(chapter=>chapterMembers(chapter).includes(actor.id)).map(recordName);
    standing.textContent=[string(actor.factionId||actor.faction),...memberships].filter(Boolean).join(' · ')||'No affiliation recorded';
    activity.append(node('span','',string(actor.activity)||'Not reported'));
    if(actor.relocating)activity.append(node('small','',`Relocating toward ${cellName(actor.relocationTarget)}`));
    else if(actor.role)activity.append(node('small','',string(actor.role)));
    row.append(identity,presence,location,standing,activity);rows.append(row);
  }
  $('#character-empty').hidden=actors.length>0;
}

function fillForm(form, values){
  form.reset();
  for(const [name,value] of Object.entries(values||{})){
    const input=form.elements.namedItem(name);if(!input)continue;
    input.value=Array.isArray(value)?value.map(item=>typeof item==='string'?item:recordId(item)).join('\n'):string(value);
  }
}
function chapterSelect(chapter){selectedChapter=chapter.id;fillForm($('#chapter-form'),{...chapter,memberIds:chapterMembers(chapter),cellIds:chapterCells(chapter),housing:chapter.housing??0,jobCapacity:chapter.jobCapacity??0,attraction:chapter.attraction??0,treatyModifier:chapter.treatyModifier??0});$('#chapter-form-title').textContent=`Edit ${recordName(chapter)}`;renderChapters();updateGuards();}
function renderChapters(){
  const values=chapters(), list=$('#chapter-list');
  if(changed('chapter-list',values)){
    list.replaceChildren();
    for(const chapter of values){const item=button(recordName(chapter),()=>chapterSelect(chapters().find(value=>value.id===chapter.id)||chapter),{key:`chapter:${chapter.id}`});item.dataset.chapterId=chapter.id;list.append(item);}
    if(!values.length)empty(list,'No Chapter records yet.');
  }
  for(const item of list.querySelectorAll('[data-chapter-id]'))item.classList.toggle('active',item.dataset.chapterId===selectedChapter);
  const dashboard=$('#chapter-dashboard'),chapter=values.find(value=>value.id===selectedChapter);
  if(!changed('chapter-dashboard',[selectedChapter,chapter,characters(state).map(actor=>[actor.id,actor.name,actor.online,actor.active]),state.activity,state.routes,state.opinions,factions(),cells().map(cell=>[cell.id,cell.name,cell.territory])]))return;
  dashboard.replaceChildren();
  if(!chapter){empty(dashboard,'Select a Chapter to inspect its members, observed activity, usual UTC playtimes, routes, and standing.');return;}
  dashboard.append(node('h3','',recordName(chapter)));
  if(chapter.description)dashboard.append(node('p','form-note',string(chapter.description)));
  const members=chapterMembers(chapter), online=characters(state).filter(actor=>members.includes(actor.id)&&actor.online&&!actor.npc);
  dashboard.append(definitionList([['Members',members.length],['Online now',online.length],['Housing',finite(chapter.housing)],['Jobs',finite(chapter.jobCapacity)],['Attraction',finite(chapter.attraction)],['Treaty modifier',chapter.treatyModifier??0]]));
  const memberSection=node('section','chapter-section');memberSection.append(node('h4','','Membership'));
  const tags=node('div','tag-list');for(const id of members)tags.append(node('span','tag',playerName(id)));if(!members.length)tags.append(node('p','empty-copy','No members recorded.'));memberSection.append(tags);dashboard.append(memberSection);
  const sites=node('section','chapter-section');
  const authoredSites=cells().filter(cell=>cell.territory?.chapter===chapter.id);
  sites.append(node('h4','','Chapter settlement sites'),node('p','form-note',chapterCells(chapter).map(cellName).join(' · ')||authoredSites.map(recordName).join(' · ')||'No settlement sites recorded.'));
  sites.append(node('p','form-note',string(chapter.siteAuthority)||(authoredSites.length?'Atlas/native-authored Chapter sites, reported by the authority.':'Site authority has not been reported.')));
  dashboard.append(sites);
  const activity=node('section','chapter-section');activity.append(node('h4','','Activity & usual UTC playtimes'));
  const observed=ownActivity(chapter), summary=chapter.activity || chapter.activitySummary || null;
  if(summary)activity.append(details(summary,'Activity summary',`chapter:${chapter.id}:activity`));
  if(observed.length){
    const total=observed.reduce((sum,item)=>sum+finite(item.activeMinutes),0),memberMinutes=observed.reduce((sum,item)=>sum+finite(item.memberMinutes),0);
    activity.append(node('p','form-note',`${total.toFixed(1)} active Chapter minutes · ${memberMinutes.toFixed(1)} member-minutes across ${observed.length} observed UTC windows. No unattended time is filled in.`));
    for(const item of [...observed].sort((a,b)=>finite(b.activeMinutes)-finite(a.activeMinutes)).slice(0,6))activity.append(node('p','form-note',`${['Mon','Tue','Wed','Thu','Fri','Sat','Sun'][finite(item.weekday)]||'Day unreported'} ${String(finite(item.hour)).padStart(2,'0')}:00–${String((finite(item.hour)+1)%24).padStart(2,'0')}:00 UTC · ${finite(item.activeMinutes).toFixed(1)} active min · ${finite(item.samples)} samples`));
  }
  else activity.append(node('p','empty-copy','No activity samples yet. Peak-time suggestions need observed participation; no convenient time is fabricated.'));
  const peak=chapter.peak || chapter.peakWindow || chapter.usualPlaytimes;
  if(peak)activity.append(details(peak,'Reported usual playtimes (UTC)',`chapter:${chapter.id}:playtimes`));
  activity.append(button('Suggest next peak window',()=>suggestPeak(chapter.id,true),{write:true,key:`chapter:${chapter.id}:peak`}));dashboard.append(activity);
  const routes=node('section','chapter-section');routes.append(node('h4','','Observed routes'));
  const journeys=array(state.routes).filter(route=>route.chapter===chapter.id||route.chapterId===chapter.id||members.includes(string(route.characterId||route.playerId)));
  if(journeys.length)for(const route of journeys.slice(-8).reverse())routes.append(node('p','form-note',`${cellName(route.source||route.from||route.fromCell)} → ${cellName(route.destination||route.to||route.toCell)} · ${finite(route.transitions??route.count)} observed transitions`));
  else routes.append(node('p','empty-copy','No route observations recorded.'));
  dashboard.append(routes);
  const standing=node('section','chapter-section');standing.append(node('h4','','Faction standing'));
  const opinions=array(state.opinions).filter(item=>item.targetType==='chapter'&&item.targetId===chapter.id);
  if(opinions.length)for(const opinion of opinions)standing.append(node('p','form-note',`${nameFor(factions(),opinion.factionId)} · ${finite(opinion.score)>0?'+':''}${finite(opinion.score)} · ${string(opinion.reason)}`));else standing.append(node('p','empty-copy','No faction opinions recorded.'));
  dashboard.append(standing);
}

function renderOpinionTarget(){
  const kind=$('#opinion-form').elements.namedItem('targetType').value;
  selectOptions($('#opinion-target'),kind==='chapter'?chapters():characters(state).filter(actor=>!actor.npc));
}
function renderFactions(){
  if(!changed('factions',[factions(),state.opinions,chapters(),characters(state).map(actor=>[actor.id,actor.name]),cells().map(cell=>[cell.id,cell.name,cell.territory])]))return;
  const list=$('#faction-list');list.replaceChildren();
  for(const faction of factions()){
    const row=record(recordName(faction),faction.id);
    if(faction.description)row.append(node('p','',string(faction.description)));
    const claims=cells().filter(cell=>worldClaims(cell).includes(faction.id));
    row.append(node('p','form-note',claims.length?`Territory claims: ${claims.map(recordName).join(' · ')}`:'No territory claims reported.'));list.append(row);
  }
  if(!list.childElementCount)empty(list,'No factions reported or authored.');
  const opinions=$('#opinion-list');opinions.replaceChildren();
  for(const opinion of array(state.opinions)){
    const target=opinion.targetType==='chapter'?nameFor(chapters(),opinion.targetId):playerName(opinion.targetId);
    const row=record(`${nameFor(factions(),opinion.factionId)} → ${target}`,opinion.targetType,`${finite(opinion.score)>0?'+':''}${finite(opinion.score)}`);
    row.append(node('p','',string(opinion.reason)||'No reason recorded.'));opinions.append(row);
  }
  if(!opinions.childElementCount)empty(opinions,'No manual opinions have been recorded.');
}

function renderStories(){
  if(!changed('stories',[state.campaigns,state.beats]))return;
  const campaigns=$('#campaign-list'), beats=$('#beat-list');campaigns.replaceChildren();beats.replaceChildren();$('#campaign-count').textContent=String(array(state.campaigns).length);
  for(const campaign of array(state.campaigns)){
    const item=record(recordName(campaign),campaign.id,'planning');
    if(campaign.description)item.append(node('p','',string(campaign.description)));
    const actions=node('div','button-row');actions.append(button('Edit',()=>{fillForm($('#campaign-form'),campaign);$('#campaign-form-title').textContent='Edit campaign';$('#campaign-form').elements.namedItem('name').focus();},{key:`campaign:${campaign.id}:edit`}),button('Delete',()=>sendCommand('campaign.delete',{campaignId:campaign.id},{title:'Delete campaign?',copy:'The service refuses deletion if story beats or events still reference this campaign. Nothing is cascaded.',label:'Delete campaign'}),{write:true,className:'danger',key:`campaign:${campaign.id}:delete`}));item.append(actions);campaigns.append(item);
  }
  if(!campaigns.childElementCount)empty(campaigns,'No campaign records. Begin with a name and a premise.');
  for(const beat of array(state.beats)){
    const item=record(recordName(beat),`${nameFor(array(state.campaigns),beat.campaignId)} · ${string(beat.kind)||'story'}`,EXECUTABLE.has(beat.kind)?'planning':'planned only');
    if(beat.description)item.append(node('p','',string(beat.description)));
    const actions=node('div','button-row');actions.append(button('Edit',()=>{fillForm($('#beat-form'),beat);$('#beat-form-title').textContent='Edit story beat';$('#beat-form').elements.namedItem('title').focus();},{key:`beat:${beat.id}:edit`}),button('Delete',()=>sendCommand('beat.delete',{beatId:beat.id},{title:'Delete story beat?',copy:'Referenced beats cannot be deleted until their event references are resolved.',label:'Delete beat'}),{write:true,className:'danger',key:`beat:${beat.id}:delete`}));item.append(actions);beats.append(item);
  }
  if(!beats.childElementCount)empty(beats,'No story beats recorded. Plans do not create active world entities.');
}

function field(labelText,name,type='text',options={}){
  const label=node('label','',labelText), input=type==='textarea'?node('textarea'):type==='select'?node('select'):node('input');
  input.name=name;
  if(type!=='textarea'&&type!=='select')input.type=type;
  if(options.required!==false)input.required=true;
  if(options.options)for(const [value,text] of options.options){const option=node('option','',text);option.value=value;input.append(option);}
  if(options.source)input.dataset.options=options.source;
  if(options.value!==undefined)input.value=options.value;
  for(const attribute of ['min','max','step','maxLength','placeholder','rows'])if(options[attribute]!==undefined)input[attribute]=options[attribute];
  label.append(input);return label;
}
function renderEventFields(){
  const kind=$('#event-kind').value, container=$('#event-fields'), form=$('#event-form'), planned=!EXECUTABLE.has(kind);container.replaceChildren();
  $('#event-executor-note').hidden=!planned;
  $('#event-executor-note').textContent='Planned only — executor unavailable. This can be saved as a story beat within a campaign, but cannot spawn attackers, command an army, assassinate a character, or collapse a faction.';
  form.querySelector('button[type=submit]').textContent=planned?'Save planned story beat':'Create event draft';
  form.elements.namedItem('campaignId').required=planned;
  form.querySelector('fieldset').hidden=planned;
  form.querySelector('fieldset').disabled=planned;
  if(planned){container.append(field('Plan and intended consequences','plan','textarea',{rows:5,maxLength:6000}));renderOptions();return;}
  if(kind==='notice'){
    container.append(field('Audience','scope','select',{options:[['world','All connected players'],['cell','Players in a cell'],['player','One connected player'],['chapter','Connected Chapter members']]}),field('Target','target','select',{required:false}),field('Narrative text','text','textarea',{rows:5,maxLength:2000}));
    form.elements.namedItem('scope').addEventListener('change',renderNoticeTarget);
  } else if(kind==='weather') {
    container.append(field('Cell','cell','select',{source:'cells'}),field('Weather preset','preset','select',{options:[['clear','Clear'],['rain','Rain'],['snow','Snow'],['fog','Fog'],['seasonal','Return to seasonal weather']]}));
  } else if(kind==='npc_relocate'){
    container.append(field('Existing NPC','npc','select',{source:'npcs'}),field('Destination cell','cell','select',{source:'cells'}));
    const pair=node('div','form-grid');pair.append(field('Local X','x','number',{min:0,max:256,step:.1,value:.5}),field('Local Y','y','number',{min:0,max:256,step:.1,value:.5}));container.append(pair,node('p','form-note','Starts real navigation; it does not teleport or create a resident. Recruited wolves and protected essential jobs cannot be reassigned. Arrival must be verified.'));
  } else if(kind==='economy_transfer'){
    container.append(field('From existing account','from','select',{source:'accounts'}),field('To existing account','to','select',{source:'accounts'}),field('Optional goods','item','select',{options:[['','No goods — coins only'],['herbs','Herbs'],['meal','Meals']]}));
    const pair=node('div','form-grid');pair.append(field('Item quantity','quantity','number',{min:0,max:99,step:1,value:0}),field('Whole coins','coins','number',{min:0,max:1000000,step:1,value:0}));container.append(pair,node('p','form-note','Transfers existing money and stock in one direction. The source must have enough. No minting, debt, negative amounts, or arbitrary items.'));
  }
  renderOptions();updateGuards();
}
function renderNoticeTarget(){
  const form=$('#event-form'),scope=form.elements.namedItem('scope'),target=form.elements.namedItem('target');if(!scope||!target)return;
  target.parentElement.hidden=scope.value==='world';target.required=scope.value!=='world';
  if(scope.value!=='world')selectOptions(target,scope.value==='cell'?cells():scope.value==='chapter'?chapters():characters(state).filter(actor=>!actor.npc&&actor.online));
}
function renderTiming(){
  const value=$('#event-timing').value;$('#event-time-label').hidden=value==='immediate';$('#event-time').required=value!=='immediate';$('#peak-controls').hidden=value!=='peak';
}
async function suggestPeak(chapterId,showAtDesk=false){
  if(!chapterId)throw new Error('Choose a related Chapter before requesting a playtime suggestion.');
  await sendCommand('chapter.peak',{chapterId},{confirm:false,onResult:result=>{
    peakSuggestion=result.suggestion || null;
    if(showAtDesk){setPage('events');$('#event-form').elements.namedItem('chapterId').value=chapterId;$('#event-timing').value='peak';renderTiming();}
    const box=$('#peak-result');box.replaceChildren();
    if(!peakSuggestion){box.textContent=string(result.reason)||'Not enough observed activity to suggest a playtime. Choose a UTC time explicitly instead.';return;}
    box.append(node('span','',`${utcLabel(peakSuggestion.scheduledAt)} · ${finite(peakSuggestion.observedMinutes)} observed minutes from ${finite(peakSuggestion.samples)} samples. `));
    box.append(button('Use this UTC time',()=>{
      $('#event-time').value=inputUtc(peakSuggestion.scheduledAt);
      showMessage('Suggested UTC time copied into the draft. Nothing is scheduled until you create and separately approve the event.');
    }));
  }});
}
function renderEvents(){
  const events=array(state.events),filter=$('#event-filter').value,list=$('#event-list');
  if(!changed('events',[events,filter]))return;
  list.replaceChildren();
  metrics($('#event-summary'),[[events.filter(e=>/draft|plan|blocked/.test(e.status||'')).length,'Draft / blocked'],[events.filter(e=>/scheduled|queued|approved|pending|relocating/.test(e.status||'')).length,'Awaiting completion'],[events.filter(e=>e.status==='applied').length,'Verified applied']]);
  const displayed=events.filter(event=>filter==='all'||filter==='queued'&&/scheduled|queued|approved|pending|relocating/.test(event.status||'')||event.status===filter).sort((a,b)=>(utcDate(b.createdAt)?.getTime()||0)-(utcDate(a.createdAt)?.getTime()||0));
  for(const event of displayed){
    const supported=EXECUTABLE.has(event.kind),status=string(event.status)||'draft';
    const item=record(string(event.title)||recordName(event),`${EVENT_LABELS[event.kind]||string(event.kind)} · ${event.scheduledAt?utcLabel(event.scheduledAt):'As soon as explicitly approved'}`,supported?status:'planned only');
    if(event.detail||event.error||event.message)item.append(node('p','',string(event.detail||event.error||event.message)));
    if(!supported)item.append(node('p','form-note','Executor unavailable. This record cannot become an active encounter or an applied world effect.'));
    if(event.kind==='npc_relocate'&&/queued|approved|pending|relocating/.test(status))item.append(node('p','form-note','Navigation accepted is not arrival. Final application waits for the authority to verify the resident reached its new home.'));
    item.append(details(event.payload||{},'Effect payload',`event:${event.id}:payload`));
    const actions=node('div','button-row');
    if(supported&&status==='draft')actions.append(button('Approve execution',()=>sendCommand('event.approve',{eventId:event.id},{title:'Approve event execution?',copy:'This authorizes the effect below at its scheduled time, or immediately if that time has already arrived. It is not merely saving a draft.',review:{eventId:event.id,title:event.title,kind:event.kind,payload:event.payload,scheduledAt:event.scheduledAt},label:'Approve event'}),{write:true,key:`event:${event.id}:approve`}));
    if(['draft','scheduled','blocked'].includes(status))actions.append(button('Cancel',()=>sendCommand('event.cancel',{eventId:event.id},{title:'Cancel event?',copy:'Ask the service to cancel this undispatched event. Queued and applied effects cannot be rolled back by canceling.',label:'Cancel event'}),{write:true,className:'danger',key:`event:${event.id}:cancel`}));
    if(actions.childElementCount)item.append(actions);list.append(item);
  }
  if(!displayed.length)empty(list,events.length?'No events match this filter.':'No event drafts or effects have been recorded.');
}

function renderMigration(){
  const container=$('#migration-results'),previews=array(state.migrationPreviews),preview=previews.find(value=>value.id===selectedPreview)||[...previews].sort((a,b)=>(utcDate(b.createdAt)?.getTime()||0)-(utcDate(a.createdAt)?.getTime()||0))[0];
  const expired=Boolean(utcDate(preview?.expiresAt)&&utcDate(preview.expiresAt).getTime()<=Date.now());
  if(!changed('migration',[preview,expired,array(state.events).filter(event=>event.migration?.previewId===preview?.id),chapters(),factions(),cells().map(cell=>[cell.id,cell.name])]))return;
  if(!preview){empty(container,'No preview available. Choose a Chapter to evaluate actual candidate residents and available capacity.');return;}
  selectedPreview=preview.id;container.replaceChildren();
  const summary=node('article','card');summary.append(node('h3','',`Preview · ${nameFor(chapters(),preview.chapterId)}`));
  const capacity=preview.capacity||{};
  summary.append(definitionList([['Housing',capacity.housing??'Not reported'],['Jobs',capacity.jobs??'Not reported'],['Occupied',capacity.occupied??'Not reported'],['Reserved',capacity.reserved??'Not reported'],['Available',capacity.available??'Not reported'],['Created',utcLabel(preview.createdAt)],['Expires',utcLabel(preview.expiresAt)]]));
  if(preview.blockedReason)summary.append(node('p','callout',string(preview.blockedReason)));
  summary.append(node('p','form-note','Availability and source-faction consequences are preview estimates. Approval reserves capacity and queues real navigation; adoption and resentment follow verified arrival.'));container.append(summary);
  for(const candidate of array(preview.candidates)){
    const item=node('article','card');item.append(node('h3','',string(candidate.name)||playerName(candidate.npcId)));
    item.append(definitionList([['Candidate',candidate.npcId],['From',cellName(candidate.sourceCell)],['Destination',cellName(candidate.destinationCell)],['Arrival',`${finite(candidate.x)}, ${finite(candidate.y)}`],['Pull score',candidate.score??'Not reported'],['Source factions',array(candidate.sourceFactions).map(value=>typeof value==='string'?nameFor(factions(),value):recordName(value)).join(', ')||'None reported'],['Standing impact',!array(candidate.sourceFactions).length?'None: no source claim':typeof candidate.resentment==='number'?`−${candidate.resentment} per source faction`:candidate.resentment?JSON.stringify(candidate.resentment):'Not reported']]));
    for(const reason of array(candidate.reasons))item.append(node('p','form-note',string(reason)));
    const migrationEvent=array(state.events).find(event=>event.migration?.previewId===preview.id&&event.payload?.npc===candidate.npcId);
    const alreadyApproved=Boolean(candidate.approved||migrationEvent);
    const blocked=Boolean(expired||preview.blockedReason||candidate.blockedReason||candidate.eligible===false||alreadyApproved);
    if(candidate.blockedReason)item.append(node('p','callout',string(candidate.blockedReason)));
    if(expired)item.append(node('p','callout','This preview has expired. Generate a fresh preview before approving.'));
    if(migrationEvent)item.append(node('p','form-note',`Migration event: ${string(migrationEvent.status)}. ${string(migrationEvent.detail)}`));
    item.append(button(alreadyApproved?'Approval already recorded':'Review migration approval',()=>sendCommand('migration.approve',{previewId:preview.id,npcId:candidate.npcId},{title:`Approve ${string(candidate.name)||candidate.npcId}'s migration?`,copy:`This queues real relocation to ${nameFor(chapters(),preview.chapterId)}. Capacity is rechecked, and source-faction resentment applies only after verified arrival.`,review:{previewId:preview.id,chapterId:preview.chapterId,capacity:preview.capacity,candidate},label:'Approve migration'}),{write:true,disabled:blocked,className:'primary',key:`migration:${preview.id}:${candidate.npcId}`}));container.append(item);
  }
  if(!array(preview.candidates).length)container.append(node('p','empty-copy','No eligible migration candidates were returned. No resident has been created or moved.'));
}
function renderAudit(){
  const rows=$('#audit-rows'),query=$('#audit-search').value.toLowerCase();
  if(!changed('audit',[state.audit,query]))return;
  rows.replaceChildren();
  const values=array(state.audit).filter(item=>JSON.stringify(item).toLowerCase().includes(query)).sort((a,b)=>finite(b.sequence)-finite(a.sequence)).slice(0,200);
  for(const item of values){
    const row=node('tr'),when=node('td','',utcLabel(item.at||item.timestamp||item.createdAt||item.atUnix)),action=node('td','',string(item.action||item.kind)||'Unreported action'),status=node('td'),detail=node('td');
    status.append(badge(auditStatus(item)));
    detail.append(node('span','',string(item.detail||item.message||item.reason)||'See recorded payload.'),node('small','',string(item.target||item.commandId||item.id)||`Audit #${finite(item.sequence)}`));detail.append(details(item,'Audit record',`audit:${item.sequence}`));
    row.append(when,action,status,detail);rows.append(row);
  }
  $('#audit-empty').hidden=values.length>0;
}

function formValues(form){return Object.fromEntries(new FormData(form));}
function bindForm(id,handler){$(id).addEventListener('submit',event=>{event.preventDefault();const form=event.currentTarget;if(!form.reportValidity())return;const values=formValues(form);Promise.resolve().then(()=>handler(values)).catch(showError);});}
bindForm('#chapter-form',values=>sendCommand('chapter.upsert',{...(values.id?{id:values.id}:{}),name:values.name.trim(),description:string(values.description).trim(),memberIds:uniqueIds(values.memberIds),cellIds:uniqueIds(values.cellIds),housing:integer(values.housing,'Housing',0,10000),jobCapacity:integer(values.jobCapacity,'Jobs',0,10000),attraction:integer(values.attraction,'Attraction',0,100),treatyModifier:numeric(values.treatyModifier,'Treaty modifier',0,1)},{title:'Save Chapter record?',copy:'This edits membership, settlement sites, and declared migration capacity. It does not construct buildings, create jobs, or change territorial claims.',onResult:result=>{const value=result.chapter||result;if(value?.id)chapterSelect(value);}}));
bindForm('#faction-form',values=>sendCommand('faction.upsert',{name:values.name.trim()},{title:'Create faction record?',copy:'Add an operator faction record. This does not spawn members or armies.',onResult:()=>$('#faction-form').reset()}));
bindForm('#opinion-form',values=>sendCommand('opinion.set',{factionId:values.factionId,targetType:values.targetType,targetId:values.targetId,score:integer(values.score,'Standing',-100,100),reason:values.reason.trim()},{title:'Record faction opinion?',copy:'This sets the faction’s opinion of the selected Chapter or player and records your reason.'}));
bindForm('#campaign-form',values=>sendCommand('campaign.upsert',{...(values.id?{id:values.id}:{}),name:values.name.trim(),description:values.description.trim()},{title:'Save campaign?',copy:'Campaign planning is private operator metadata and has no live effect.',onResult:()=>{$('#campaign-form').reset();$('#campaign-form-title').textContent='New campaign';}}));
bindForm('#beat-form',values=>sendCommand('beat.upsert',{...(values.id?{id:values.id}:{}),campaignId:values.campaignId,title:values.title.trim(),description:values.description.trim(),kind:values.kind.trim()},{title:'Save story beat?',copy:'This records an intended story, not an executed encounter.',onResult:()=>{$('#beat-form').reset();$('#beat-form-title').textContent='New story beat';}}));
bindForm('#event-form',async values=>{
  if(!EXECUTABLE.has(values.kind)){
    await sendCommand('beat.upsert',{campaignId:values.campaignId,title:values.title.trim(),kind:values.kind,description:string(values.plan).trim()},{title:'Save planned story beat?',copy:'Executor unavailable. This saves planning text only; no assassin, brigand, army, or faction collapse will be created.',label:'Save plan'});return;
  }
  const payload=eventPayload(values.kind,values),scheduledAt=values.timing==='immediate'?null:parseUtcInput(values.scheduledAt);
  if(scheduledAt&&utcDate(scheduledAt).getTime()<=Date.now())throw new Error('Choose a future UTC time, or select immediate execution after approval.');
  const event={title:values.title.trim(),kind:values.kind,payload,scheduledAt};for(const field of ['campaignId','beatId','chapterId'])if(values[field])event[field]=values[field];
  await sendCommand('event.create',event,{title:'Create an event draft?',copy:'This saves the proposed effect and time. A separate approval is required before the effect can execute.',label:'Create draft'});
});
bindForm('#migration-form',values=>sendCommand('migration.preview',{chapterId:values.chapterId},{confirm:false,onResult:result=>{const preview=result.preview||result;if(preview?.id){selectedPreview=preview.id;state.migrationPreviews=[...array(state.migrationPreviews).filter(value=>value.id!==preview.id),preview];renderMigration();}}}));

for(const element of $$('[data-page]'))element.addEventListener('click',()=>setPage(element.dataset.page));
$('#connect').addEventListener('click',()=>$('#session-dialog').showModal());
$('#session-cancel').addEventListener('click',()=>$('#session-dialog').close());
$('#session-form').addEventListener('submit',event=>{event.preventDefault();connectSession($('#session-token').value).catch(showError);});
$('#disconnect').addEventListener('click',disconnectSession);
$('#refresh').addEventListener('click',refreshState);
$('#confirm-accept').addEventListener('click',()=>finishConfirmation(true));
$('#confirm-cancel').addEventListener('click',()=>finishConfirmation(false));
$('#confirm-dialog').addEventListener('cancel',event=>{event.preventDefault();finishConfirmation(false);});
$('#fit-map').addEventListener('click',fitMap);
for(const id of ['layer-claims','layer-chapters','layer-actors','map-level'])$(`#${id}`).addEventListener('change',drawMap);
$('#cell-search').addEventListener('input',renderWorld);
$('#character-search').addEventListener('input',renderCharacters);
$('#character-filter').addEventListener('change',renderCharacters);
$('#audit-search').addEventListener('input',renderAudit);
$('#event-filter').addEventListener('change',()=>{renderEvents();updateGuards();});
$('#event-kind').addEventListener('change',renderEventFields);
$('#event-timing').addEventListener('change',renderTiming);
$('#opinion-form').elements.namedItem('targetType').addEventListener('change',renderOpinionTarget);
$('#suggest-peak').addEventListener('click',()=>suggestPeak($('#event-form').elements.namedItem('chapterId').value).catch(showError));
$('#chapter-reset').addEventListener('click',()=>{selectedChapter='';$('#chapter-form').reset();$('#chapter-form-title').textContent='Create a Chapter record';renderChapters();updateGuards();});
$('#campaign-reset').addEventListener('click',()=>{$('#campaign-form').reset();$('#campaign-form-title').textContent='New campaign';});
$('#beat-reset').addEventListener('click',()=>{$('#beat-form').reset();$('#beat-form-title').textContent='New story beat';});

const canvas=$('#world-canvas');
canvas.addEventListener('wheel',event=>{event.preventDefault();map.zoom=Math.max(.35,Math.min(12,map.zoom*(event.deltaY<0?1.15:1/1.15)));drawMap();},{passive:false});
canvas.addEventListener('pointerdown',event=>{if(event.button!==0&&event.button!==1)return;map.drag={x:event.clientX,y:event.clientY,panX:map.panX,panY:map.panY};map.moved=false;canvas.setPointerCapture(event.pointerId);});
canvas.addEventListener('pointermove',event=>{if(!map.drag)return;const dx=event.clientX-map.drag.x,dy=event.clientY-map.drag.y;if(Math.hypot(dx,dy)>4)map.moved=true;map.panX=map.drag.panX+dx;map.panY=map.drag.panY+dy;drawMap();});
canvas.addEventListener('pointerup',event=>{if(!map.drag)return;if(!map.moved){const bounds=canvas.getBoundingClientRect(),x=event.clientX-bounds.left,y=event.clientY-bounds.top;const hit=[...map.hitCells].reverse().find(cell=>x>=cell.x&&x<=cell.x+cell.w&&y>=cell.y&&y<=cell.y+cell.h);if(hit){selectedCell=hit.id;renderWorld();}}map.drag=null;});
canvas.addEventListener('pointercancel',()=>{map.drag=null;});
canvas.addEventListener('keydown',event=>{if(event.key==='Home'){event.preventDefault();fitMap();}else if(event.key==='+'||event.key==='='){event.preventDefault();map.zoom=Math.min(12,map.zoom*1.15);drawMap();}else if(event.key==='-'){event.preventDefault();map.zoom=Math.max(.35,map.zoom/1.15);drawMap();}});
new ResizeObserver(()=>drawMap()).observe(canvas);

// Additional Chapter fields are real service metadata, kept when editing an existing profile.
const chapterForm=$('#chapter-form');
const descriptionField=field('Description','description','textarea',{required:false,rows:3,maxLength:6000});
chapterForm.elements.namedItem('name').parentElement.after(descriptionField);
chapterForm.querySelector('.form-note').before(field('Treaty mitigation · 0 none, 1 full','treatyModifier','number',{min:0,max:1,step:.05,value:0}));
$('#opinion-form').elements.namedItem('reason').maxLength=1000;
$('#beat-form').elements.namedItem('kind').maxLength=64;
renderEventFields();renderTiming();render();
setInterval(()=>{if(token)refreshState();},3000);
setInterval(updateGuards,1000);
const fragment=new URLSearchParams(location.hash.slice(1));
const initialToken=fragment.get('token');
if(initialToken){history.replaceState(null,'',`${location.pathname}${location.search}`);connectSession(initialToken).catch(showError);}
