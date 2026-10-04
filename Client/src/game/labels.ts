// The words the game screen shows for the world's state (UI/SRatwGame.cpp): pace, posture, wind, weather, the
// calendar and the moon, elevation and scent. Pure functions of what the server sent.
import {bool, clamp, envNumber, num, obj, str, wholeCount, type Maybe} from './json.ts';

const Compass = ['E', 'SE', 'S', 'SW', 'W', 'NW', 'N', 'NE'];
export const compassName = (sector: number) => Compass[clamp(Math.trunc(sector), 0, 7)];

export function paceLabel(pace: number): string {
    return pace <= 0 ? 'WALK' : pace <= 5 ? 'TROT' : pace <= 8 ? 'RUN' : 'SPRINT';
}

export function postureLabel(self: Maybe): string {
    const posture = str(self, 'posture', 'standing');
    const remaining = num(self, 'postureRemaining');
    if (remaining > 0) return `rising to ${str(self, 'postureTarget', 'standing')} · ${remaining.toFixed(1)}s`;
    if (posture === 'crouching') return bool(self, 'moving') ? 'crouching · sneaking' : 'crouching · low profile';
    return posture + (bool(self, 'turning') ? ' · turning' : '');
}

// Rest under way (doc 38): toward a full rest in a bed, or a partial one anywhere else; "" when not resting.
export function restLabel(self: Maybe): string {
    const rest = obj(self, 'rest');
    if (!rest) return '';
    const hours = num(rest, 'hours'), full = num(rest, 'full', 6);
    if (!bool(rest, 'bed')) return '  ·  resting (no bed: a partial rest)';
    return hours >= full ? '  ·  fully rested' : `  ·  resting in a bed · ${hours.toFixed(1)} of ${full} h`;
}

// Crime and law: held in the gaol (and for how long), or wanted by a town's watch (and what they ask).
export function lawLabel(self: Maybe): string {
    const custody = obj(self, 'custody');
    if (custody) return `HELD IN THE GAOL · ${Math.ceil(num(custody, 'seconds') / 60)} min`;
    const wanted = obj(self, 'wanted');
    if (wanted) return `WANTED · ${str(wanted, 'charges')} · owes ${Math.trunc(num(wanted, 'owed'))}p`;
    return '';
}

export interface ScentCue {
    sector: number;
    strength: number;
    windborne: boolean;
}

export function scentLabel(cues: readonly ScentCue[]): string {
    if (!cues.length) return 'SCENT · no unseen scent detected';
    const directions = cues.slice(0, 2).map(c => compassName(c.sector)).join(' / ') + (cues.length > 2 ? ' …' : '');
    return `SCENT · unseen wolf roughly ${directions}${cues.some(c => c.windborne) ? ' · upwind' : ''}`;
}

export function windLabel(outdoors: boolean, strength: number, direction: number, variable: boolean): string {
    if (!outdoors) return 'SHELTERED · still air';
    if (strength <= 0.01) return 'AIR FLOW · calm';
    const to = (Math.round(direction / (Math.PI / 4)) + 8) % 8;
    const force = strength < 0.3 ? 'light' : strength < 0.7 ? 'breeze' : 'strong';
    return `AIR FLOW · ${compassName((to + 4) % 8)} -> ${compassName(to)} · ${force}${variable ? ' · shifting' : ''}`;
}

const pad = (n: number) => String(n).padStart(2, '0');

/** The weather where the wolf stands, with how strong it is there ("LIGHT RAIN", "HEAVY SNOW"). */
export function weatherWords(weather: string, intensity?: number): string {
    if (weather === 'clear') return 'CLEAR';
    if (intensity === undefined) return weather.toUpperCase();
    if (weather === 'overcast') return intensity < 0.45 ? 'THIN CLOUD' : 'OVERCAST';
    const word = weather.toUpperCase();
    return intensity < 0.35 ? `LIGHT ${word}` : intensity > 0.75 ? `HEAVY ${word}` : word;
}

export function environmentLabel(hour: number, phase: string, weather: string, outdoors: boolean, intensity?: number): string {
    const minutes = Math.floor(hour * 60) % (24 * 60);
    return `${pad(Math.floor(minutes / 60))}:${pad(minutes % 60)} ${phase.toUpperCase()} · ${outdoors ? weatherWords(weather, intensity) : 'SHELTERED'}`;
}

const calendarOf = (snapshot: Maybe) => obj(obj(obj(snapshot, 'cell'), 'environment'), 'calendar');

export function calendarLabel(snapshot: Maybe): string {
    const calendar = calendarOf(snapshot);
    const year = wholeCount(calendar, 'year'), day = wholeCount(calendar, 'dayOfYear', -1, 365);
    const seasonDay = wholeCount(calendar, 'dayOfSeason', -1, 92);
    const season = str(calendar, 'season').toLowerCase();
    if (year < 1 || day < 1 || seasonDay < 1 || !['spring', 'summer', 'autumn', 'winter'].includes(season)) return 'THE SHARED WORLD';
    const weekday = str(calendar, 'weekday').toUpperCase();
    return `YEAR ${year} · ${weekday ? weekday + ' · ' : ''}${season.toUpperCase()} ${seasonDay} · DAY ${day} / 365`;
}

// What kind of day it is where the player stands (Phase 9): nothing on an ordinary one.
export function dayLabel(snapshot: Maybe): string {
    const day = obj(obj(snapshot, 'cell'), 'day');
    const kind = str(day, 'kind');
    const foul = bool(day, 'foul');
    if (kind === 'market') return foul ? 'MARKET DAY · NO STALLS TODAY' : 'MARKET DAY · STALLS OUT';
    if (kind === 'rest') return 'REST DAY';
    if (kind === 'festival') return `${str(day, 'name').toUpperCase()}${foul ? ' · KEPT INDOORS' : ' · FESTIVAL'}`;
    return '';
}

const Moons = ['new moon', 'waxing crescent', 'first quarter', 'waxing gibbous', 'full moon', 'waning gibbous',
    'last quarter', 'waning crescent'];

export function moonLabel(snapshot: Maybe): string {
    const calendar = calendarOf(snapshot);
    if (!calendar) return '';
    const moon = str(calendar, 'moonName').toLowerCase();
    if (!Moons.includes(moon)) return 'MOON · UNKNOWN';
    return `${moon.toUpperCase()} · ${Math.round(envNumber(calendar, 'moonIllumination', 0, 1, 0) * 100)}% LIT`;
}

export interface EnvironmentView {
    weather: string;
    intensity: number;          // How strong the weather is where the wolf stands (0..1).
    phase: string;
    lightingTone: string;
    lightSource: string;
    hour: number;
    daylight: number;
    illumination: number;
    artificialLight: number;
    daylightAccess: number;
    glowStrength: number;
    sight: number;
    hearing: number;
    scent: number;
    movement: number;
}

export function environmentEffectsLabel(e: EnvironmentView, outdoors: boolean, reducedMotion: boolean): string {
    const exposure = outdoors ? 'EXPOSURE'
        : e.illumination < 0.25 ? 'UNLIT SHELTER'
            : e.glowStrength > 0.05 ? `${e.lightingTone.toUpperCase()} LIGHT` : 'SHELTERED';
    const p = (v: number) => Math.round(v * 100);
    return `${exposure} · SIGHT ${p(e.sight)}%  HEARING ${p(e.hearing)}%  SCENT ${p(e.scent)}%  FOOTING ${p(e.movement)}%` +
        (reducedMotion ? ' · STATIC WEATHER' : '');
}

export function elevationLabel(height: number): string {
    const halves = Math.round(Math.abs(height) * 2);
    const amount = halves === 0 ? '0' : halves === 1 ? '½' : halves % 2 === 0 ? String(halves / 2) : `${Math.floor(halves / 2)}½`;
    return `GROUND ${halves === 0 ? '' : height > 0 ? '+' : '−'}${amount}`;
}

/** A tile's height from its character in a "heights" row: '0'..'p' are -16..16 by halves. */
export function heightFromChar(c: string): number {
    const code = c.charCodeAt(0);
    return code >= 48 && code <= 112 ? (code - 48 - 32) / 2 : 0;
}

// A scene's pay, in words (doc 08): what one still needs, from the server's own counts, or that one is on track.
export function sceneNeedsLabel(scene: Maybe): string {
    const turns = num(scene, 'needTurns'), words = num(scene, 'needWords'), reply = bool(scene, 'needReply');
    const needs: string[] = [];
    if (turns > 0) needs.push(`${turns} more ${turns === 1 ? 'line' : 'lines'} of five words or more`);
    if (words > 0) needs.push(`${words} more ${words === 1 ? 'word' : 'words'}`);
    if (reply) needs.push('answer someone');
    const fight = bool(scene, 'fight');
    if (needs.length) return `${fight ? 'To be paid twice' : 'To be paid'}: ${needs.join(' · ')}`;
    if (num(scene, 'othersShaped') < 1) return "You've said enough: waiting for another to say as much";
    return fight ? 'Talked through: paid twice' : 'On track to be paid';
}

// A quiet scene's warning (doc 08): when it ends unless someone speaks; "" while it isn't quiet.
export function sceneQuietLabel(scene: Maybe): string {
    if (!bool(scene, 'quiet') || bool(scene, 'fight')) return '';
    const minutes = Math.max(1, Math.ceil(num(scene, 'endsIn') / 60));
    return `Quiet · ends in ${minutes} min unless someone speaks`;
}
