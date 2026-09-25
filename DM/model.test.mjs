import test from 'node:test';
import assert from 'node:assert/strict';
import {bridgeFresh, normalizeCells, mergedRecords, uniqueIds, parseUtcInput, inputUtc, utcLabel, integer, numeric, eventPayload, EXECUTABLE, statusClass, auditStatus, commandMessage} from './model.mjs';

test('freshness expires locally even when no further request completes', () => {
  const state = {bridge:{live:true,stale:false,ageSeconds:2}};
  assert.equal(bridgeFresh(state,1000,2000),true);
  assert.equal(bridgeFresh(state,1000,10001),false);
  assert.equal(bridgeFresh({...state,bridge:{...state.bridge,stale:true}},1000,1000),false);
  assert.equal(bridgeFresh({bridge:{live:true}},1000,1000),false);
  assert.equal(bridgeFresh(null,1000,1000),false);
});
test('UTC entry is independent of the operator browser timezone', () => {
  assert.equal(parseUtcInput('2026-09-21T19:30'),'2026-09-21T19:30:00.000Z');
  assert.equal(inputUtc('2026-09-21T19:30:00Z'),'2026-09-21T19:30');
  assert.equal(utcLabel('2026-09-21T19:30:00Z'),'2026-09-21 19:30:00 UTC');
  assert.throws(() => parseUtcInput('2026-02-30T19:30'));
  assert.throws(() => parseUtcInput('later'));
});
test('normalizes real cells without inserting sample places', () => {
  assert.deepEqual(normalizeCells({}),[]);
  assert.deepEqual(normalizeCells({cells:[{id:'a',name:'A',x:5,y:6,z:1,width:32,height:24}]}),[{id:'a',name:'A',x:5,y:6,z:1,width:32,height:24}]);
  assert.equal(normalizeCells({cells:[{id:'b',worldX:3,worldY:4,worldZ:2,width:0,height:8}]})[0].width,1);
});
test('merges operator metadata by identity without losing native fields', () => {
  assert.deepEqual(mergedRecords([{id:'a',name:'Updated'}],[{id:'a',name:'Old',x:3}]),[{id:'a',name:'Updated',x:3}]);
  assert.deepEqual(uniqueIds('a, b\na b'),['a','b']);
});
test('numeric form guards reject fractions, missing and nonfinite values', () => {
  assert.equal(integer('3','Value',0,100),3);
  for (const value of ['3.5','',Infinity,-1,101]) assert.throws(() => integer(value,'Value',0,100));
  assert.equal(numeric('3.5','Value',0,100),3.5);
  assert.throws(() => numeric('NaN','Value',0,100));
});
test('only four executable effect families can be composed', () => {
  assert.equal(EXECUTABLE.size,4);
  for (const kind of ['brigands','assassins','war','faction-collapse']) assert.throws(() => eventPayload(kind,{}));
});
test('narrative notices require real text and targeted scopes require targets', () => {
  assert.deepEqual(eventPayload('notice',{scope:'world',text:' A message '}),{scope:'world',text:'A message'});
  assert.throws(() => eventPayload('notice',{scope:'world',text:' '}));
  assert.throws(() => eventPayload('notice',{scope:'chapter',text:'Welcome'}));
  assert.deepEqual(eventPayload('notice',{scope:'chapter',target:'chapter',text:'Welcome'}),{scope:'chapter',target:'chapter',text:'Welcome'});
});
test('relocation requires an existing selection and finite coordinates', () => {
  assert.deepEqual(eventPayload('npc_relocate',{npc:'n',cell:'a',x:'3.5',y:'4'}),{npc:'n',cell:'a',x:3.5,y:4});
  assert.throws(() => eventPayload('npc_relocate',{npc:'n',cell:'a',x:'Infinity',y:4}));
  assert.throws(() => eventPayload('npc_relocate',{cell:'a',x:3,y:4}));
});
test('economy composer does not offer minting, self transfers or negative stock', () => {
  assert.deepEqual(eventPayload('economy_transfer',{from:'treasury',to:'p',coins:5,quantity:0,item:''}),{from:'treasury',to:'p',coins:5,quantity:0,item:''});
  for (const values of [{from:'a',to:'a',coins:1,quantity:0},{from:'a',to:'b',coins:-1,quantity:0},{from:'a',to:'b',coins:0,quantity:0},{from:'a',to:'b',coins:0,quantity:1,item:''}]) assert.throws(() => eventPayload('economy_transfer',values));
});
test('queue and failure presentation are distinct from applied', () => {
  assert.equal(statusClass('queued'),'warn');
  assert.equal(statusClass('failed'),'error');
  assert.equal(statusClass('applied'),'');
  assert.equal(statusClass('planned'),'info');
  assert.equal(statusClass('scheduled'),'warn');
});
test('audit command acceptance is not falsely represented as world application',()=>{
  assert.equal(auditStatus({action:'event.approve'}),'recorded');
  assert.equal(auditStatus({action:'event.create',ok:true}),'recorded');
  assert.equal(auditStatus({action:'event.applied'}),'applied');
  assert.equal(auditStatus({action:'event.queued'}),'queued');
  assert.equal(auditStatus({action:'rejected:event.approve'}),'rejected');
});
test('planning saves have committed-record feedback, distinct from queued world effects',()=>{
  assert.match(commandMessage('chapter.upsert'),/profile saved/);
  assert.doesNotMatch(commandMessage('campaign.upsert'),/queued/);
  assert.match(commandMessage('event.create',{status:'draft'}),/separate approval/);
  assert.match(commandMessage('event.approve',{status:'queued'}),/queued is not applied/);
  assert.match(commandMessage('migration.approve'),/verified arrival/);
});
