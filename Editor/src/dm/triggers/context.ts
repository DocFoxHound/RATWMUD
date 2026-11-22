// What every part of the trigger workbench shares (Docs/Design/85, Phase 4): the catalogue, the world's references for the
// pickers, names, the program and the one way to edit it.
import {createContext, useContext} from 'react';
import type {Project} from '../../model/model.mjs';
import type {Catalogue, Names, Program, Refs} from '../../lib/workbench';

export interface Bench {
    cat: Catalogue; refs: Refs | null; names: Names; program: Program; world: Project | null;
    /** A new program; edits with the same `mark` (typing in one field) are one step of undo. */
    edit: (next: Program, mark?: string) => void;
    /** Problems by place: "t:<phase>:<index>" (a trigger's head), "…:actions[2]", "…:if"; "x:<phase>:<index>" (an exit). */
    problems: Record<string, string[]>;
}
export const BenchContext = createContext<Bench | null>(null);
export function useBench(): Bench {
    const b = useContext(BenchContext);
    if (!b) throw Error('The workbench context is missing.');
    return b;
}
