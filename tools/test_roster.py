#!/usr/bin/env python3
"""Roster rules: permanent, unique profession assignment and safe LLM generation."""
import copy
import json
from pathlib import Path
import tempfile
import unittest

import map_editor as editor
import roster as R


def roster(*characters):
    prof = lambda i, b: {'id': i, 'name': i.title(), 'behavior': b, 'workLabel': 'working', 'description': '',
                         'hours': {'start': 8, 'end': 17}, 'paid': True}
    return {'format': 'ratw-roster', 'version': 1,
            'professions': [prof('guard', 'guard'), prof('shopkeeper', 'merchant'), prof('farmer', 'civilian')],
            'characters': list(characters)}


def char(cid, **prefs):
    return {'id': cid, 'name': cid.title(), 'age': 30, 'voice': 1, 'description': '', 'personality': 'Calm.',
            'traits': ['steady'], 'backstory': 'Born somewhere.', 'greeting': 'Hello.', 'preferences': prefs,
            'status': 'active', 'profession': '', 'assignment': None, 'origin': 'manual',
            'appearance': {'species': 'timber', 'sex': 'male', 'stature': 'average', 'pattern': 'solid',
                           'baseColor': 1, 'gradientColor': 2, 'markingColor': 3}}


def greyfen():
    return editor.read_json((editor.ROOT / 'Data/Worlds/Greyfen/greyfen.atlas.json').read_text(encoding='utf-8'))


def slot(sid, profession, x=30, y=20):
    spot = {'cell': 'town', 'x': x, 'y': y}
    return {'id': sid, 'name': sid.replace('_', ' '), 'profession': profession, 'workLabel': '', 'hours': {'start': 8, 'end': 17},
            'route': '', 'paid': True, 'purse': 20, 'herbs': 0, 'meals': 1, 'home': spot, 'work': spot, 'evening': spot}


class AssignmentTests(unittest.TestCase):
    def world(self, *slots):
        p = greyfen()
        p['slots'] = list(slots)
        return p

    def test_best_preference_wins_and_is_stable(self):
        r = roster(char('ada', guard=1), char('bo', guard=3), char('cy', guard=3))
        updated, plan, warnings = R.assign(self.world(slot('gate', 'guard')), r)
        self.assertEqual(plan, [{'slot': 'gate', 'character': 'bo', 'new': True}])
        self.assertEqual(warnings, [])
        again, plan2, _ = R.assign(self.world(slot('gate', 'guard')), updated)
        self.assertEqual(plan2, [{'slot': 'gate', 'character': 'bo', 'new': False}])
        self.assertEqual(again, updated)

    def test_one_place_only_and_profession_is_for_life(self):
        r = roster(char('bo', guard=3, farmer=3))
        updated, plan, _ = R.assign(self.world(slot('gate', 'guard'), slot('field', 'farmer')), r)
        self.assertEqual([e['character'] for e in plan], ['bo'])
        bo = updated['characters'][0]
        self.assertEqual((bo['profession'], bo['assignment']), ('guard', {'world': 'greyfen', 'slot': 'gate'}))
        # Slot deleted: released, but still a guard for life.
        released, plan, warnings = R.assign(self.world(slot('field', 'farmer')), updated)
        self.assertEqual(plan, [])
        self.assertIsNone(released['characters'][0]['assignment'])
        self.assertEqual(released['characters'][0]['profession'], 'guard')
        self.assertTrue(warnings)
        _, plan, _ = R.assign(self.world(slot('wall', 'guard')), released)
        self.assertEqual(plan[0]['character'], 'bo')

    def test_characters_never_appear_in_two_worlds(self):
        r = roster(char('bo', guard=3))
        updated, _, _ = R.assign(self.world(slot('gate', 'guard')), r)
        other = self.world(slot('gate', 'guard'))
        other['id'] = 'elsewhere'
        _, plan, warnings = R.assign(other, updated)
        self.assertEqual(plan, [])
        self.assertIn('no free character', warnings[0])

    def test_dead_and_removed_are_never_drawn_and_free_their_slot(self):
        r = roster(char('bo', guard=3), char('cy', guard=2))
        updated, _, _ = R.assign(self.world(slot('gate', 'guard')), r)
        updated['characters'][0]['status'] = 'dead'
        after, plan, _ = R.assign(self.world(slot('gate', 'guard')), updated)
        self.assertEqual(plan[0]['character'], 'cy')
        self.assertIsNone(after['characters'][0]['assignment'])
        after['characters'][1]['status'] = 'removed'
        _, plan, _ = R.assign(self.world(slot('gate', 'guard')), after)
        self.assertEqual(plan, [])

    def test_zero_preference_and_named_ids_are_excluded(self):
        r = roster(char('bo', guard=0, farmer=2), char('wren', guard=3))
        _, plan, _ = R.assign(self.world(slot('gate', 'guard')), r)
        self.assertEqual(plan, [], 'bo refuses guard work; wren is already a named NPC in Greyfen')

    def test_retrading_a_slot_releases_its_holder(self):
        r = roster(char('bo', guard=3), char('fa', farmer=3))
        updated, _, _ = R.assign(self.world(slot('gate', 'guard')), r)
        after, plan, _ = R.assign(self.world(slot('gate', 'farmer')), updated)
        self.assertEqual(plan[0]['character'], 'fa')
        self.assertIsNone(next(c for c in after['characters'] if c['id'] == 'bo')['assignment'])

    def test_validation_rejects_double_booking_and_bad_data(self):
        a, b = char('a', guard=3), char('b', guard=3)
        for c in (a, b):
            c['profession'], c['assignment'] = 'guard', {'world': 'w', 'slot': 's'}
        with self.assertRaises(R.RosterError):
            R.check_roster(roster(a, b))
        for bad in ({'preferences': {'wizard': 3}}, {'preferences': {'guard': 9}}, {'status': 'asleep'},
                    {'id': 'treasury'}, {'assignment': {'world': 'w', 'slot': 's'}}, {'traits': ['x' * 41]}):
            with self.subTest(bad=bad), self.assertRaises(R.RosterError):
                R.check_roster(roster({**char('z', guard=1), **bad}))


class ExportTests(unittest.TestCase):
    def test_export_persists_assignments_and_emits_story(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'roster.json'
            R.save(roster(char('bo', guard=3)), path)
            p = greyfen()
            p['slots'] = [slot('gate', 'guard')]
            files, plan = editor.export_and_assign(p, path)
            manifest = files['world.ratw']
            self.assertIn('resident "bo" "Bo" "guard"', manifest)
            self.assertIn('story "bo" "Calm. Traits: steady." "Born somewhere."', manifest)
            self.assertIn('story "wren"', manifest, 'named NPCs carry their story too')
            saved = R.load(path)
            self.assertEqual(saved['characters'][0]['assignment'], {'world': 'greyfen', 'slot': 'gate'})
            self.assertEqual(json.loads(files['atlas.json'])['slots'][0]['id'], 'gate', 'source atlas unchanged')

    def test_merchant_slots_need_a_customer_tile(self):
        p = greyfen()
        p['slots'] = [slot('stall', 'shopkeeper', 5, 5)]
        p['terrain'] = [row[:4] + ('#.#' if y == 5 else '###') + row[7:] if 4 <= y <= 6 else row for y, row in enumerate(p['terrain'])]
        with self.assertRaises(editor.ValidationError):
            editor.export_files(p, roster(char('bo', shopkeeper=3)))


class GenerationTests(unittest.TestCase):
    def test_model_output_is_clamped_into_a_valid_character(self):
        r = roster(char('bo', guard=3))
        raw = {'name': 'Bo', 'age': 900, 'species': 'dragon', 'sex': 'x', 'stature': 'tall', 'pattern': 'solid',
               'baseColor': 44, 'gradientColor': -1, 'markingColor': 2, 'description': 'd\x00' * 5, 'personality': 'p',
               'traits': ['a', '', 'b' * 99], 'backstory': 'b', 'greeting': 'line\nbreak', 'preferences': {'guard': 7, 'wizard': 3}}
        c = R._normalize_generated(raw, r, ['guard', 'shopkeeper', 'farmer'], 0)
        self.assertNotEqual(c['id'], 'bo', 'never reuses an existing ID')
        self.assertEqual((c['age'], c['appearance']['species'], c['appearance']['baseColor']), (90, 'timber', 7))
        self.assertEqual(c['preferences'], {'guard': 3})
        self.assertEqual(c['origin'], 'llm')
        R.check_roster(roster(char('bo', guard=3), c))

    def test_generate_uses_fixed_endpoint_and_parses(self):
        calls = []

        class Config:
            model, api_key, endpoint = 'test-model', 'secret', 'https://api.openai.com/v1/chat/completions'

        def fake(config, payload, timeout, bridge):
            calls.append(payload)
            content = {'characters': [{'name': 'Ash', 'age': 40, 'species': 'arctic', 'sex': 'female', 'stature': 'short',
                                       'pattern': 'saddle', 'baseColor': 0, 'gradientColor': 1, 'markingColor': 2,
                                       'description': 'Pale.', 'personality': 'Wry.', 'traits': ['wry'], 'backstory': 'Old.',
                                       'greeting': 'Well?', 'preferences': {'guard': 2, 'shopkeeper': 0, 'farmer': 1}}]}
            return json.dumps({'choices': [{'finish_reason': 'stop', 'message': {'content': json.dumps(content)}}]}).encode()
        original = R._post_openai
        R._post_openai = fake
        try:
            made = R.generate(Config(), roster(), 1, ['guard'], 'river town')
        finally:
            R._post_openai = original
        self.assertEqual(made[0]['name'], 'Ash')
        self.assertNotIn('secret', json.dumps(calls[0]), 'the key is never placed in the request body')
        self.assertEqual(calls[0]['response_format']['json_schema']['strict'], True)
        with self.assertRaises(R.RosterError):
            R.generate(Config(), roster(), 9, [], '')


if __name__ == '__main__':
    unittest.main(verbosity=1)
