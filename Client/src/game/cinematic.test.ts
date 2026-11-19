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
