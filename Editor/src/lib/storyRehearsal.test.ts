import {test} from 'node:test';
import assert from 'node:assert/strict';
import {checkStory, exportStory, importStory, inspectRun, newStory, type Rehearsal} from './storyRehearsal.ts';

function ferry() {
    const story = newStory();
    story.title = 'The late ferry'; story.place = 'quay';
    story.steps = [
        {id:'wait', name:'Wait', journal:'The clerk lights a lamp.', ending:false, exits:[{when:'arrival',to:'land'},{when:'deadline',to:'late'}]},
        {id:'land', name:'Land', journal:'The boat ties up.', ending:true, exits:[]},
        {id:'late', name:'Delay', journal:'The tide has turned.', ending:true, exits:[]}
    ];
    return story;
}
test('a mismatched arrival and a repeated delivery do not expose the other ending', () => {
    const story = ferry();
    assert.deepEqual(checkStory(story), []);
    const run: Rehearsal = {story, events:[{id:1, when:'arrival',place:'upstream'},{id:2,when:'deadline',place:''},{id:2,when:'deadline',place:''}]};
    const result = inspectRun(JSON.parse(JSON.stringify(run)));
    assert.deepEqual(result.journal, ['The clerk lights a lamp.', 'The tide has turned.']);
    assert.match(result.log[1], /different location/); assert.match(result.log[3], /Repeated/);
    assert.equal(inspectRun({story, events:[{id:1,when:'arrival',place:'quay'}]}).step.id,'land');
});
test('validation explains missing destinations, cast locations, unreachable steps and closed loops', () => {
    const story = ferry();
    story.place = '';
    story.steps[0].exits[0].to = 'missing';
    story.steps[0].exits[1].to = 'wait';
    const issues = checkStory(story).join('\n');
    for (const problem of ['Choose the next step', 'Choose a location', 'no path to an ending', 'unreachable'])
        assert.ok(issues.includes(problem), issues);
});

test('a transferred story gets its own ID and keeps both branches', () => {
    const original = ferry(), copy = importStory(exportStory(original));
    assert.notEqual(copy.id, original.id);
    assert.deepEqual({...copy, id:original.id}, original);
    const malformed = JSON.parse(exportStory(original));
    malformed.draft.steps[0].exits[0].when = 'unknown';
    assert.throws(() => importStory(JSON.stringify(malformed)), /unknown event/);
});
