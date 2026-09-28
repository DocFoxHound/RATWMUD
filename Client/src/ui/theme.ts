// The game's palette and type, as the Unreal client had them (UI/SRatwGame.cpp, SRatwFrontDoor.cpp).
import {rgb, type Color} from './color.ts';

export const Ink = rgb(0x11191b), Panel = rgb(0x182122), Raised = rgb(0x24302d);
export const Line = rgb(0x34443e), Paper = rgb(0xded5c3), Muted = rgb(0x8b9b91);
export const Amber = rgb(0xd9b67b), Sage = rgb(0xa8c2a6), Blue = rgb(0x92bacd);
export const Scent = rgb(0xb6a3cf);

const Speaking = [0xDAC69D, 0xA8C7AD, 0xA8CEDA, 0xCEAEE0, 0xE1ABA2, 0xE6C481, 0xAEC4E4, 0xD5AFC6,
    0xBECF91, 0x8BD2C6, 0xD8BAA3, 0xD0D7AD, 0x9DB9D6, 0xC5B4E4, 0xE2AEB5, 0xD7CB98,
    0xB2D0BE, 0xA4C9C4, 0xA8BDE2, 0xD4AFE0, 0xD7A68F, 0xD9BC76, 0x95C3A0, 0x8EC4D7,
    0xB5A3D2, 0xD39CC0, 0xCAB497, 0xC0CFA9, 0x97BCB5, 0xB2C7D1, 0xCCC3DC, 0xE0D6BF];

/** One of the 32 speaking colours a player chooses from. */
export function speakingColor(index: number): Color {
    return rgb(Speaking[Math.min(31, Math.max(0, Math.trunc(index) || 0))]);
}

export const CoatNames = ['Ivory', 'Silver', 'Ash', 'Stone', 'Sable', 'Charcoal', 'Rust', 'Sand'];
export const CoatColors = [0xE1D9C6, 0xADB3B2, 0x777D7B, 0x8E8271, 0x65513F, 0x303534, 0xA26843, 0xBEAA84];

// Slate sizes type in points at 96 DPI: a size of 12 is 16 pixels.
export const pixels = (size: number) => size * 4 / 3;
export const SansFamily = "'RATW Sans', Roboto, 'Segoe UI', Arial, sans-serif";
export const MonoFamily = "'RATW Mono', 'DejaVu Sans Mono', monospace";

export function font(size: number, mono = false, bold = false): string {
    return `${bold && !mono ? 500 : 400} ${pixels(size)}px ${mono ? MonoFamily : SansFamily}`;
}
