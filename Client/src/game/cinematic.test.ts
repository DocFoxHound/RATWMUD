import assert from 'node:assert/strict';
import test from 'node:test';
import {actorPoint} from './cinematic.ts';
import {testGame} from './testing.ts';

test('only the presented beat can accept a vote or reply and a locked scene cancels held input', () => {
    const {state:s,commands} = testGame();
    const offer = {instance:'run-1',beat:'ask',type:'choice',lockMovement:true,remainingMs:5000,canRespond:true,
        options:[{id:'wait',label:'Wait'},{id:'enter',label:'Enter'}]};
    s.keyDown({code:'KeyW'});
    s.applySnapshot({self:{scene:offer}});
    assert.equal(s.heldKeys.size,0);
    const n = commands.length;
    s.keyDown({code:'KeyD'}); s.send({type:'path',x:3,y:1});
    assert.equal(commands.length,n);
    s.cinematic.vote('another',0,s.send); s.cinematic.vote('wait',0,s.send); s.cinematic.vote('enter',.1,s.send);
    assert.deepEqual(commands.slice(n),[{type:'scene_vote',instance:'run-1',beat:'ask',option:'wait'}]);
    s.applySnapshot({self:{scene:{...offer,type:'response',beat:'explain',canRespond:false}}});
    s.cinematic.reply('Not eligible',1,s.send);
    s.applySnapshot({self:{scene:{...offer,type:'response',beat:'explain',canRespond:true}}});
    s.cinematic.reply(' ',1,s.send); s.cinematic.reply('x'.repeat(281),1,s.send);
    s.cinematic.reply(' Wheels stuck in mud. ',1,s.send);
    assert.deepEqual(commands.slice(n+1),[{type:'scene_reply',instance:'run-1',beat:'explain',text:'Wheels stuck in mud.'}]);
    s.applySnapshot({self:{scene:{...offer,beat:'next',remainingMs:0}}});
    s.cinematic.vote('wait',1,s.send);
    assert.equal(commands.length,n+2);
    s.applySnapshot({self:{}});
    s.keyDown({code:'KeyD'});
    assert.ok(s.heldKeys.has('KeyD'));
    assert.equal(s.cinematic.current,null);
});

test('private cast positions interpolate separately from the world and clear when a viewer leaves',()=>{
    const {state:s}=testGame();
    const watch=(x:number)=>s.applySnapshot({self:{scene:{instance:'one',beat:'walk',type:'move',
        suppressedActors:['off-camera'], actors:[{id:'messenger',name:'Messenger',x,y:4.5,cell:'palace'}]}}});
    watch(2.5);
    s.clock=.2; watch(3.5);
    assert.deepEqual(actorPoint(s.cinematic.actors.get('messenger')!,.30),[3,4.5]);
    assert.equal(s.entities.has('messenger'),false);
    assert.ok(s.cinematic.suppressedActors.has('off-camera') && !s.cinematic.actors.has('off-camera'));
    s.applySnapshot({self:{}});
    assert.equal(s.cinematic.actors.size,0);
    assert.equal(s.cinematic.suppressedActors.size,0);
});

test('each speaker has their own panel: the scene\'s lines and players\' heard words, newest last', () => {
    const {state:s} = testGame();
    const face = {fur:'grey'};
    const lines = [{key:'1',kind:'resident',speakerId:'u1',name:'Harrow',appearance:face,text:'Who keeps the ford?'}];
    const at = (beat:string, more:object[]=[]) => s.applySnapshot({self:{id:'player-ash',scene:{instance:'one',beat,type:'response',
        remainingMs:5000,canRespond:true,lines:[...lines,...more]}}});
    at('ask');
    assert.deepEqual(s.cinematic.speeches().map(l=>[l.kind,l.name,l.text]),[['resident','Harrow','Who keeps the ford?']]);
    s.clock=1;
    at('ask',[{key:'2',kind:'player',speakerId:'player-ash',name:'Ash',appearance:face,text:'I will.',self:true}]);
    s.clock=2;
    s.cinematic.hear('post-9',{id:'player-fern',name:'Fern',appearance:face,artwork:'',self:false},'"And I."',2);
    s.cinematic.hear('post-9',{id:'player-fern',name:'Fern',appearance:face,artwork:'',self:false},'"And I."',2);
    s.clock=3;
    at('close',[{key:'2',kind:'player',speakerId:'player-ash',name:'Ash',appearance:face,text:'I will.',self:true},
        {key:'3',kind:'player',name:'Someone',text:'Aye.'}, {key:'4',kind:'resident',speakerId:'u1',name:'Harrow',text:'Then it is kept.'}]);
    const shown = s.cinematic.speeches();
    assert.deepEqual(shown.map(l=>l.text),['I will.','"And I."','Aye.','Then it is kept.']);
    assert.ok(shown[0]!.self && !shown[1]!.self && shown[2]!.speakerId==='' && shown[2]!.appearance===null,
        'an unseen speaker has a panel with no face, never a guess');
    s.applySnapshot({self:{id:'player-ash',scene:{instance:'two',beat:'a',type:'line',lines:[]}}});
    assert.equal(s.cinematic.speeches().length,0,'a new scene starts with no panels');
    s.applySnapshot({self:{}});
    s.cinematic.hear('post-10',{id:'player-fern',name:'Fern',appearance:face,artwork:'',self:false},'Hello',4);
    assert.equal(s.cinematic.speeches().length,0,'outside a scene speech stays in the chat');
});

test('a player\'s own words said in the chat during a scene stand in their own panel', () => {
    const {state:s} = testGame();
    s.applySnapshot({self:{id:'player-ash',name:'Ash',scene:{instance:'one',beat:'ask',type:'line',remainingMs:5000,lines:[]}}});
    s.receiveEvent({type:'roleplay',id:'e1',speaker:'Ash',speakerId:'player-ash',appearance:{fur:'grey'},text:'"We go at dawn."'});
    s.receiveEvent({type:'roleplay',id:'e2',speaker:'Myrtle',speakerId:'u4',appearance:{fur:'red'},text:'"Fresh bread."'});
    assert.deepEqual(s.cinematic.speeches().map(l=>[l.name,l.text,l.self]),[['Ash','"We go at dawn."',true]],
        'a resident\'s bark outside the cast stays in the chat');
});
