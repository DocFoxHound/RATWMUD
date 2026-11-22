// What the "New small quest" flow is given in the tests (doc 85, Phase 6): the carter at the ford needs rope. The program it
// makes is kept beside this file (small-quest.json) for tools/ to check and rehearse.
import type {Program, SmallQuestInput} from '../workbench.ts';

export const SMALL_QUEST_INPUT: SmallQuestInput = {
    title: 'Rope for the carter', summary: 'The carter at the ford has lost his load to a snapped rope.', giver: 'rm111',
    objectives: [{label: 'Ask the carter what he needs.', on: 'conversation', match: {target: 'rm111'}},
        {label: 'Bring him two coils of rope.', on: 'gift', match: {target: 'rm111', item: 'rope'}, goal: 2, measure: 'quantity'}],
    reward: {kind: 'coins', coins: 12, purse: 'town:greyfen', reason: "the carter's thanks"},
    offer: {say: 'My rope snapped at the ford and the load went in. Could you find me some?', accept: "I'll find you some rope.", handIn: 'Here is your rope.',
        thanks: "That's me moving again. Thank you."},
    repeat: 'weekly',
};

/** The small quest with a campaign trigger that uses the quest effects (offer, start, progress, reveal) and a story choice. */
export function questCampaignOf(smallQuest: Program): Program {
    return {...smallQuest, quests: [...smallQuest.quests!, {id: 'thief', title: 'Who cut the rope?', scope: 'party',
    objectives: [{id: 'ask', label: 'Ask about the rope.', on: 'conversation'}, {id: 'name', label: 'Name the one who cut it.', on: 'dialogue choice',
        match: {detail: 'accuse'}, hidden: true}, {id: 'spare', label: 'A spare coil.', on: 'gift', optional: true}],
    fail: [{on: 'quest completed', match: {detail: 'rope-for-the-carter'}}, {afterDays: 3}], rewards: [{kind: 'deed.award', player: '$actor', detail: 'Found the rope-cutter.'}]}],
    triggers: [{id: 'on-done', on: 'quest completed', audience: 'participant', match: {detail: 'rope-for-the-carter'}, actions: [
        {do: 'effect', effect: {kind: 'quest.offer', quest: 'thief', player: '$actor'}},
        {do: 'effect', effect: {kind: 'objective.reveal', quest: 'thief', objective: 'name', player: '$actor'}},
        {do: 'effect', effect: {kind: 'objective.progress', quest: 'rope-for-the-carter', objective: 'step-2', player: '$actor', amount: 1}},
        {do: 'effect', effect: {kind: 'npc.offer', npc: 'rm111', id: 'accuse', words: 'It was the ferryman.', reply: 'The ferryman? I never.'}},
        {do: 'effect', effect: {kind: 'participant.add', player: '$actor'}}]}]}
}
